#ifndef WRITER_H
#define WRITER_H

#include <stdbool.h>
#include <stdio.h>

#include "types.h"

writer *writer_new(int fd);
void writer_free(writer *obj);
void writer_begin(writer *writer);
void writer_flush(writer *writer);
void writer_write(writer *writer, void const *buf, size_t count);
void writer_write_header(writer *w, u8 type, u16 request_id, u16 content_length);

#endif
