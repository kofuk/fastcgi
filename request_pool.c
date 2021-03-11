#include "request_pool.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"
#include "types.h"
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

static void initialize_request(request *req, request_pool *pool,
                               u16 request_id) {
    req->request_id = request_id;
    req->headers = hashtable_new();
    req->stdin = vector_new();
    req->fd = pool->write_fd;
    req->initialized = true;
}

void request_pool_add(request_pool *pool, u16 request_id) {
    /* If there's empty slot in request array,
       reuse it. */
    for (size_t i = 0; i < pool->size; ++i) {
        if (!pool->requests[i].initialized) {
            initialize_request(&pool->requests[i], pool, request_id);
            return;
        }
    }

    if (pool->cap <= pool->size) {
        pool->cap <<= 1;
        pool->requests = realloc(pool->requests, pool->cap * sizeof(request));
    }
    initialize_request(&pool->requests[pool->size], pool, request_id);
    pool->size++;
}

request *request_pool_get(request_pool *pool, u16 request_id) {
    for (size_t i = 0; i < pool->size; ++i) {
        if (pool->requests[i].request_id == request_id &&
            pool->requests[i].initialized) {
            return &pool->requests[i];
        }
    }
    return NULL;
}

void request_pool_erase(request_pool *pool, u16 request_id) {
    for (size_t i = 0; i < pool->size; ++i) {
        if (pool->requests[i].initialized) {
            if (pool->requests[i].request_id == request_id) {
                hashtable_free(pool->requests[i].headers);
                vector_free(pool->requests[i].stdin, true);
                pool->requests[i].initialized = false;
            }
        }
    }
}
