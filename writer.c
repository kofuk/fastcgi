#include "writer.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "endian.h"
#include "fcgi_proto.h"
#include "response.h"
#include "types.h"

struct writer {
    int fd;
    u8 buf[2048];
    size_t nbuffered;
};

writer *writer_new(int fd) {
    writer *result = malloc(sizeof(writer));
    memset(result, 0, sizeof(writer));
    result->fd = fd;
    return result;
}

static inline bool internal_flush(writer *writer) {
    size_t off = 0;
    while (writer->nbuffered > 0) {
        ssize_t n = write(writer->fd, writer->buf + off, writer->nbuffered);
        if (n < 0) {
            return false;
        }
        writer->nbuffered -= n;
        off += n;
    }
    return true;
}

void writer_begin(writer *writer) {}

void writer_flush(writer *writer) { internal_flush(writer); }

void writer_free(writer *obj) {
    if (obj->nbuffered != 0) {
        writer_flush(obj);
    }

    free(obj);
}

void writer_write(writer *writer, void const *buf, size_t count) {
    while (count > 0) {
        if (writer->nbuffered == 2048) {
            internal_flush(writer);
        }
        size_t copy_count;
        if (count > 2048 - writer->nbuffered) {
            copy_count = 2048 - writer->nbuffered;
        } else {
            copy_count = count;
        }
        memcpy(writer->buf + writer->nbuffered, buf, copy_count);
        count -= copy_count;
        buf += copy_count;
        writer->nbuffered += copy_count;
    }
}

void writer_write_header(writer *w, u8 type, u16 request_id,
                         u16 content_length) {
    fcgi_header hdr = {0};
    hdr.version = FCGI_VERSION_1;
    hdr.type = type;
    write_u16(request_id, &hdr.request_id);
    write_u16(content_length, &hdr.content_length);
    writer_write(w, &hdr, sizeof(fcgi_header));
}
