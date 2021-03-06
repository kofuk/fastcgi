#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

typedef struct request_pool request_pool;
typedef struct hashtable hashtable;

typedef struct {
    u8 *data;
    size_t len;
    size_t cap;
} vector;

typedef struct {
    u16 request_id;
    hashtable *headers;
    vector *stdin;
    u8 flags;
    bool initialized;
} request;

typedef struct {
    char *key;
    char *val;
} pair;

typedef struct response response;

typedef struct writer writer;

#endif
