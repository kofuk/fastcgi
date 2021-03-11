#ifndef RESPONSE_H
#define RESPONSE_H

#include "types.h"

response *response_new(void);
void response_add_header(response *resp, char const *key, char const *val);
void response_terminate_header(response *resp);
vector *response_get_header(response *resp);
void response_write(response *resp, u8 const *data, size_t len);
vector *response_get_body(response *resp);
void response_free(response *obj);

#endif
