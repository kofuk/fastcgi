#ifndef WRITER_H
#define WRITER_H

#include <stdbool.h>
#include <stdio.h>

#include "types.h"

writer *writer_new(int fd);
void writer_free(writer *obj, bool close_fd);
void writer_write(writer *writer, void const *buf, size_t count);
void writer_flush(writer *writer);

#endif
