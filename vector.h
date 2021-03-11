#ifndef VECTOR_H
#define VECTOR_H

#include <stdbool.h>

#include "types.h"

vector *vector_new(void);
void vector_free(vector *obj, bool free_content);
void vector_append(vector *vec, u8 const *data, size_t len);

#endif
