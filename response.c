#include "response.h"

#include <stdlib.h>
#include <string.h>

#include "vector.h"

struct response {
    vector *header;
    vector *content;
};

response *response_new(void) {
    response *result = malloc(sizeof(response));
    result->header = vector_new();
    result->content = vector_new();
    return result;
}

void response_add_header(response *resp, char const *key, char const *val) {
    vector_append(resp->header, (u8 const*)key, strlen(key));
    vector_append(resp->header, (u8 const *)": ", 2);
    vector_append(resp->header, (u8 const *)val, strlen(val));
    vector_append(resp->header, (u8 const *)"\r\n", 2);
}

vector *response_get_header(response *resp) {
    return resp->header;
}

void response_write(response *resp, u8 const *data, size_t len) {
    vector_append(resp->content, data, len);
}

vector *response_get_body(response *resp) {
    return resp->content;
}

void response_free(response *obj) {
    vector_free(obj->header, true);
    vector_free(obj->content, true);
}
