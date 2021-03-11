#include "request_pool.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"
#include "vector.h"

struct request_pool {
    request *requests;
    size_t cap;
    size_t size;
    int write_fd;
};

request_pool *request_pool_new(int write_fd) {
    request_pool *result = malloc(sizeof(request_pool));
    memset(result, 0, sizeof(request_pool));
    result->cap = 2;
    result->requests = malloc(result->cap * sizeof(request));
    result->write_fd = write_fd;

    return result;
}

void request_pool_free(request_pool *obj) {
    free(obj->requests);
    free(obj);
}

void request_pool_add(request_pool *pool,  u16 request_id) {
    if (pool->cap <= pool->size) {
        pool->cap <<= 1;
        pool->requests = realloc(pool->requests, pool->cap * sizeof(request));
    }
    memset(&pool->requests[pool->size], 0, sizeof(request));
    pool->requests[pool->size].request_id = request_id;
    pool->requests[pool->size].headers = hashtable_new();
    pool->requests[pool->size].stdin = vector_new();
    pool->requests[pool->size].fd = pool->write_fd;
    pool->size++;
}

request *request_pool_get(request_pool *pool, u16 request_id) {
    for (size_t i = 0; i < pool->size; ++i) {
        if (pool->requests[i].request_id == request_id) {
            return &pool->requests[i];
        }
    }
    return NULL;
}

void request_pool_erase(request_pool *pool, u16 request_id) {
    
}
