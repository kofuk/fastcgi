#include "fcgi_proto.h"

#include <string.h>

#include "endian.h"

void *make_end_request_body(fcgi_body_end_request *out, u32 app_status,
                            u8 protocol_status) {
    memset(out, 0, sizeof(fcgi_body_end_request));
    write_u32(app_status, &out->app_status);
    out->protocol_status = protocol_status;
    return out;
}

void *make_unknown_type_body(fcgi_body_unknown_type *out, u8 type) {
    memset(out, 0, sizeof(fcgi_body_unknown_type));
    out->type = type;
    return out;
}
