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
#include "writer.h"

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

            used += local_used;
            n -= local_used;
        }
    next:
        if (n <= used) {
            memcpy(buf, buf + used, n);
        } else {
            memmove(buf, buf + used, n);
        }
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

static bool write_response(writer *writer, u16 request_id, response *resp) {
    writer_begin(writer);

    vector *head = response_get_header(resp);
    vector *body = response_get_body(resp);

    u8 *data = malloc(head->len + body->len + 2);
    memcpy(data, head->data, head->len);
    memcpy(data + head->len, "\r\n", 2);
    memcpy(data + head->len + 2, body->data, body->len);
    size_t len = head->len + body->len + 2;
    size_t wrote = 0;

    while (wrote < len) {
        size_t nwrite;
        if (len - wrote > 65535) {
            nwrite = 65535;
        } else {
            nwrite = len;
        }

        writer_write_header(writer, FCGI_TYPE_STDOUT, request_id, nwrite);
        writer_write(writer, data + wrote, nwrite);
        wrote += nwrite;
    }

    writer_write_header(writer, FCGI_TYPE_STDOUT, request_id, 0);

    free(data);

    writer_write_header(writer, FCGI_TYPE_END_REQUEST, request_id,
                        (u16)sizeof(fcgi_body_end_request));
    fcgi_body_end_request end_body;
    writer_write(
        writer,
        make_end_request_body(&end_body, 0, FCGI_STATUS_REQUEST_COMPLETE),
        sizeof(fcgi_body_end_request));

    writer_flush(writer);

    return true;
}

static void handle_fcgi(int fd) {
    writer *writer = writer_new(fd);
    request_pool *pool = request_pool_new(writer);

    for (;;) {
        fcgi_header hdr;
        if (!read_fcgi_header(fd, &hdr)) {
            break;
        }

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
                writer_begin(writer);
                writer_write_header(writer, FCGI_TYPE_END_REQUEST,
                                    hdr.request_id,
                                    sizeof(fcgi_body_end_request));
                fcgi_body_end_request body;
                writer_write(
                    writer,
                    make_end_request_body(&body, 0, FCGI_STATUS_UNKNOWN_ROLE),
                    (u16)sizeof(fcgi_body_end_request));
                writer_flush(writer);
                continue;
            }

            request_pool_add(pool, hdr.request_id, body.flags);
            break;
        }
        case FCGI_TYPE_ABORT_REQUEST: {
            discard_input(fd, (size_t)hdr.content_length + hdr.padding_length);

            writer_begin(writer);
            writer_write_header(writer, FCGI_TYPE_END_REQUEST, hdr.request_id,
                                sizeof(fcgi_body_end_request));
            fcgi_body_end_request body;
            writer_write(
                writer,
                make_end_request_body(&body, 0, FCGI_STATUS_REQUEST_COMPLETE),
                (u16)sizeof(fcgi_body_end_request));
            writer_flush(writer);

            request_pool_erase(pool, hdr.request_id);
            break;
        }
        case FCGI_TYPE_PARAMS: {
            request *req = request_pool_get(pool, hdr.request_id);
            if (req == NULL) {
                discard_input(fd,
                              (size_t)hdr.content_length + hdr.padding_length);
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
                continue;
            }

            collect_stdin(fd, hdr.content_length, req->stdin);
            if (hdr.padding_length != 0) {
                discard_input(fd, (size_t)hdr.padding_length);
            }

            if (hdr.content_length == 0) {
                write_response(writer, hdr.request_id, handle_request(req));
                request_pool_erase(pool, hdr.request_id);

                if (!(bool)(req->flags & FCGI_FLAG_KEEP_CONN)) {
                    goto out;
                }
            }
        }
        default: {
            writer_begin(writer);
            writer_write_header(writer, FCGI_TYPE_UNKNOWN_TYPE, hdr.request_id,
                                sizeof(fcgi_body_end_request));
            fcgi_body_unknown_type body;
            writer_write(writer, make_unknown_type_body(&body, hdr.type),
                         (u16)sizeof(fcgi_body_unknown_type));
            writer_flush(writer);
        }
        }
    }
out:
    request_pool_free(pool);
    writer_free(writer);
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

        handle_fcgi(fd);

        close(fd);
    }

    return 0;
fail:
    close(sock);
    return 1;
}
