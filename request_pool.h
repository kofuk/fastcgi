#ifndef REQUEST_POOL_H
#define REQUEST_POOL_H

#include <stdbool.h>

#include "types.h"
#include "writer.h"

request_pool *request_pool_new(writer *w);
void request_pool_free(request_pool *obj);
void request_pool_add(request_pool *pool,  u16 request_id, u8 flags);
request *request_pool_get(request_pool *pool, u16 request_id);
void request_pool_erase(request_pool *pool, u16 request_id);

#endif
