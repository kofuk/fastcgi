#include "writer.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

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

void writer_free(writer *obj, bool close_fd) {
    if (obj->nbuffered != 0) {
        writer_flush(obj);
    }

    free(obj);
}

static inline bool internal_flush(writer *writer) {
    while (writer->nbuffered > 0) {
        ssize_t n = write(writer->fd, writer->buf, writer->nbuffered);
        if (n < 0) {
            return false;
        }
        writer->nbuffered -= n;
    }
    return true;
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
    }
}

void writer_flush(writer *writer) {
    internal_flush(writer);
}
