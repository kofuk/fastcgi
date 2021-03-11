#include "vector.h"

#include <stdlib.h>
#include <string.h>

vector *vector_new(void) {
    vector *result = malloc(sizeof(vector));
    result->len = 0;
    result->cap = 8;
    result->data = malloc(result->cap);
    return result;
}

void vector_free(vector *obj, bool free_content) {
    if (free_content) {
        free(obj->data);
    }
    free(obj);
}

void vector_append(vector *vec, u8 const *data, size_t len) {
    while (vec->cap - vec->len < len) {
        vec->cap <<= 1;
        vec->data = realloc(vec->data, vec->cap);
    }

    memcpy(vec->data + vec->len, data, len);
    vec->len += len;
}
