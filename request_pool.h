#ifndef REQUEST_POOL_H
#define REQUEST_POOL_H

#include <stdbool.h>

#include "types.h"

request_pool *request_pool_new(int write_fd);
void request_pool_free(request_pool *obj);
void request_pool_add(request_pool *pool,  u16 request_id);
request *request_pool_get(request_pool *pool, u16 request_id);
void request_pool_erase(request_pool *pool, u16 request_id);

#endif
