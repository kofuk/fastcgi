#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "fcgi_proto.h"
#include "hashtable.h"
#include "request_pool.h"
#include "response.h"
#include "types.h"
#include "vector.h"

static inline void reverse_array(u8 *arr, size_t const len) {
    for (size_t i = 0; i < len / 2; ++i) {
        arr[i] ^= arr[len - i - 1];
        arr[len - i - 1] ^= arr[i];
        arr[i] ^= arr[len - i - 1];
    }
}

static void reverse_order(void *data, size_t n) {
    u8 arr[sizeof(u16)];
    memcpy(arr, data, sizeof(u16));
    reverse_array(arr, sizeof(u16));
    memcpy(data, arr, sizeof(u16));
}

static u16 reverse_order_v_16(u16 v) {
    reverse_order(&v, sizeof(u16));
    return v;
}

static bool read_fcgi_header(int fd, fcgi_header *out) {
    ssize_t nread = read(fd, out, sizeof(fcgi_header));
    if (nread < sizeof(fcgi_header)) {
        return false;
    }
    reverse_order(&out->request_id, sizeof(u16));
    reverse_order(&out->content_length, sizeof(u16));
    return true;
}

static bool read_begin_request_body(int fd, fcgi_body_begin_request *out) {
    ssize_t nread = read(fd, out, sizeof(fcgi_body_begin_request));
    if (nread < sizeof(fcgi_body_begin_request)) {
        return false;
    }
    reverse_order(&out->role, sizeof(u16));
    return true;
}

static void write_error(int fd, u16 request_id) {
    fcgi_header hdr = {0};
    hdr.type = FCGI_TYPE_END_REQUEST;
    hdr.version = FCGI_VERSION_1;
    hdr.content_length = reverse_order_v_16((u16)sizeof(fcgi_body_end_request));
    hdr.request_id = reverse_order_v_16(request_id);
    write(fd, &hdr, sizeof(fcgi_header));

    fcgi_body_end_request body = {0};
    body.app_status = 0;
    body.protocol_status = FCGI_STATUS_OVERLOADED;
    write(fd, &body, sizeof(fcgi_body_end_request));
}

static void write_unsupported_role(int fd, u16 request_id) {
    fcgi_header hdr = {0};
    hdr.type = FCGI_TYPE_END_REQUEST;
    hdr.version = FCGI_VERSION_1;
    hdr.content_length = reverse_order_v_16((u16)sizeof(fcgi_body_end_request));
    hdr.request_id = reverse_order_v_16(request_id);
    write(fd, &hdr, sizeof(fcgi_header));

    fcgi_body_end_request body = {0};
    body.app_status = 0;
    body.protocol_status = FCGI_STATUS_UNKNOWN_ROLE;
    write(fd, &body, sizeof(fcgi_body_end_request));
}

static void write_complete_request(int fd, u16 request_id) {
    fcgi_header hdr = {0};
    hdr.type = FCGI_TYPE_END_REQUEST;
    hdr.version = FCGI_VERSION_1;
    hdr.content_length = reverse_order_v_16((u16)sizeof(fcgi_body_end_request));
    hdr.request_id = reverse_order_v_16(request_id);
    write(fd, &hdr, sizeof(fcgi_header));

    fcgi_body_end_request body = {0};
    body.app_status = 0;
    body.protocol_status = reverse_order_v_16(FCGI_STATUS_REQUEST_COMPLETE);
    write(fd, &body, sizeof(fcgi_body_end_request));
}

static void discard_input(int fd, size_t len) {
    u8 buf[2048];
    while (len != 0) {
        ssize_t n = read(fd, buf, len);
        if (n < 0) {
            return;
        }

        len -= (size_t)n;
    }
}

static u32 decode_u32(u8 const *buf) {
    u8 tmp[sizeof(u32)];
    memcpy(tmp, buf, sizeof(u32));
    tmp[0] &= 0x7F;
    reverse_order(tmp, sizeof(u32));
    u32 result;
    memcpy(&result, tmp, sizeof(u32));
    return result;
}

static void encode_u32(u32 val, u8 *out) {
    memcpy(out, &val, sizeof(u32));
    reverse_order(out, sizeof(u32));
    out[0] |= 0x80;
}

static bool read_request_params(int fd, size_t content_length, hashtable *out) {
    u8 buf[2048];
    size_t off = 0;
    while (content_length != 0 || off != 0) {
        ssize_t n;
        if (content_length != 0) {
            size_t should_read;
            if (content_length > 2048 - off) {
                should_read = 2048 - off;
            } else {
                should_read = content_length;
            }
            n = read(fd, buf + off, should_read);
            if (n < 0) {
                return false;
            }
        }
        content_length -= n;
        n += off;

        size_t used = 0;
        for (;;) {
            size_t local_used = 0;
            size_t key_len;
            size_t val_len;
            if (n == 0) {
                goto next;
            } else {
                if (buf[used] >> 7 == 1) {
                    if (n < 4) {
                        goto next;
                    }
                    key_len = decode_u32(buf + used);
                    local_used += 4;
                } else {
                    key_len = *(buf + used);
                    local_used += 1;
                }
            }

            if (n - local_used == 0) {
                goto next;
            } else {
                if (buf[used + local_used] >> 7 == 1) {
                    if (n < 4) {
                        goto next;
                    }
                    val_len = decode_u32(buf + used + local_used);
                    local_used += 4;
                } else {
                    val_len = *(buf + used + local_used);
                    local_used += 1;
                }
            }

            if (n - local_used < key_len + val_len) {
                /* XXX
                   This can't support keys and values longer than 2040. */
                goto next;
            }

            char *key = malloc(key_len + 1);
            key[key_len] = '\0';
            memcpy(key, buf + used + local_used, key_len);
            local_used += key_len;

            char *val = malloc(val_len + 1);
            val[val_len] = '\0';
            memcpy(val, buf + used + local_used, val_len);
            local_used += val_len;

            hashtable_put(out, key, val);

            printf("%s=%s\n", key, val);

            used += local_used;
            n -= local_used;
        }
    next:;
        memmove(buf, buf + used, n);
        off = n;
    }

    return true;
}

static bool collect_stdin(int fd, size_t content_length, vector *out) {
    u8 buf[2048];
    while (content_length > 0) {
        size_t should_read;
        if (content_length > 2048) {
            should_read = 2048;
        } else {
            should_read = content_length;
        }
        ssize_t n = read(fd, buf, content_length);
        if (n < 0) {
            return false;
        }
        content_length -= n;
        vector_append(out, buf, (size_t)n);
    }
    return true;
}

static response *handle_request(request *req) {
    response *resp = response_new();

    response_add_header(resp, "Content-Type", "text/html");
    response_write(resp, (u8 const *)"Hello, world\n", 14);

    return resp;
}

static bool write_response(int fd, u16 request_id, response *resp) {
    fcgi_header hdr = {0};
    hdr.request_id = reverse_order_v_16(request_id);
    hdr.type = FCGI_TYPE_STDOUT;

    response_terminate_header(resp);

    vector *head = response_get_header(resp);
    vector *body = response_get_body(resp);
    if (head->len + body->len + 2 < 65536) {
        hdr.content_length = reverse_order_v_16((u16)head->len + body->len + 2);
    } else {
        // TODO: implement
        __builtin_trap();
    }

    write(fd, &hdr, sizeof(fcgi_header));
    write(fd, head->data, head->len + 2); /* Additional 2 bytes for \r\n */
    write(fd, body->data, body->len);

    hdr.type = FCGI_TYPE_END_REQUEST;
    hdr.content_length = reverse_order_v_16((u16)sizeof(fcgi_body_end_request));
    write(fd, &hdr, sizeof(fcgi_header));

    fcgi_body_end_request end_body = {0};
    end_body.protocol_status = FCGI_STATUS_REQUEST_COMPLETE;
    write(fd, &end_body, sizeof(fcgi_body_end_request));

    return true;
}

static void handle_fcgi(int fd) {
    request_pool *pool = request_pool_new(fd);

    for (;;) {
        fcgi_header hdr;
        if (!read_fcgi_header(fd, &hdr)) {
            break;
        }
        printf("version=%d, type=%d, request_id=%d, content_length=%d, "
               "padding_length=%d\n",
               hdr.version, hdr.type, hdr.request_id, hdr.content_length,
               hdr.padding_length);

        switch (hdr.type) {
        case FCGI_TYPE_BEGIN_REQUEST: {
            fcgi_body_begin_request body;
            if (!read_begin_request_body(fd, &body)) {
                goto out;
            }
            if (hdr.padding_length != 0) {
                discard_input(fd, (size_t)hdr.padding_length);
            }

            if (body.role != FCGI_ROLE_RESPONDER) {
                write_unsupported_role(fd, hdr.request_id);
                continue;
            }

            request_pool_add(pool, hdr.request_id, body.flags);
            break;
        }
        case FCGI_TYPE_ABORT_REQUEST: {
            discard_input(fd, (size_t)hdr.content_length + hdr.padding_length);
            write_complete_request(fd, hdr.request_id);
            request_pool_erase(pool, hdr.request_id);
            break;
        }
        case FCGI_TYPE_PARAMS: {
            request *req = request_pool_get(pool, hdr.request_id);
            if (req == NULL) {
                discard_input(fd,
                              (size_t)hdr.content_length + hdr.padding_length);
                write_error(fd, hdr.request_id);
                continue;
            }

            read_request_params(fd, hdr.content_length, req->headers);
            if (hdr.padding_length != 0) {
                discard_input(fd, (size_t)hdr.padding_length);
            }

            break;
        }
        case FCGI_TYPE_STDIN: {
            request *req = request_pool_get(pool, hdr.request_id);
            if (req == NULL) {
                discard_input(fd,
                              (size_t)hdr.content_length + hdr.padding_length);
                write_error(fd, hdr.request_id);
                continue;
            }

            collect_stdin(fd, hdr.content_length, req->stdin);
            if (hdr.padding_length != 0) {
                discard_input(fd, (size_t)hdr.padding_length);
            }

            if (hdr.content_length == 0) {
                write_response(fd, hdr.request_id, handle_request(req));
                request_pool_erase(pool, hdr.request_id);

                if (!(bool)(req->flags & FCGI_FLAG_KEEP_CONN)) {
                    goto out;
                }
            }
        }
        default: {
            discard_input(fd, (size_t)hdr.content_length + hdr.padding_length);
        }
        }
    }
out:
    request_pool_free(pool);
}

int main(int argc, char **argv) {
    char const *sock_path;
    struct sockaddr_un sockaddr = {0};

    if (argc < 2) {
        sock_path = "/tmp/fcgi.sock";
    } else {
        sock_path = argv[1];
    }
    if (strlen(sock_path) >= sizeof(sockaddr.sun_path)) {
        fprintf(stderr, "%s: Socket path too long", sock_path);
        return 1;
    }

    int sock;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        goto fail;
    }

    sockaddr.sun_family = AF_UNIX;
    strcpy(sockaddr.sun_path, sock_path);

    if (bind(sock, (struct sockaddr *)&sockaddr, sizeof(struct sockaddr_un)) <
        0) {
        perror("bind");
        goto fail;
    }

    if (listen(sock, 4) < 0) {
        perror("listen");
        goto fail;
    }

    for (;;) {
        int fd;
        if ((fd = accept(sock, NULL, NULL)) < 0) {
            perror("accept");
            continue;
        }

        puts("accepted!");

        handle_fcgi(fd);

        close(fd);
        puts("closed!");
    }

    return 0;
fail:
    close(sock);
    return 1;
}
