#ifndef FCGI_PROTO_H
#define FCGI_PROTO_H

#include "types.h"

#define FCGI_VERSION_1 1

typedef struct {
    u8 version;
    u8 type;
    u16 request_id;
    u16 content_length;
    u8 padding_length;
    u8 reserved;
} __attribute__((packed)) fcgi_header;

#define FCGI_TYPE_BEGIN_REQUEST 1
#define FCGI_TYPE_ABORT_REQUEST 2
#define FCGI_TYPE_END_REQUEST 3
#define FCGI_TYPE_PARAMS 4
#define FCGI_TYPE_STDIN 5
#define FCGI_TYPE_STDOUT 6
#define FCGI_TYPE_STDERR 7
#define FCGI_TYPE_DATA 8
#define FCGI_TYPE_GET_VALUES 9
#define FCGI_TYPE_GET_VALUES_RESULT 10
#define FCGI_TYPE_UNKNOWN_TYPE 11
#define FCGI_MAXTYPE (FCGI_UNKNOWN_TYPE)

#define FCGI_NULL_REQUEST_ID 0

typedef struct {
    u16 role;
    u8 flags;
    u8 reserved[5];
} __attribute__((packed)) fcgi_body_begin_request;

#define FCGI_FLAG_KEEP_CONN 1

#define FCGI_ROLE_RESPONDER 1
#define FCGI_ROLE_AUTHORIZER 2
#define FCGI_ROLE_FILTER 3

typedef struct {
    u32 app_status;
    u8 protocol_status;
    u8 reserved[3];
} fcgi_body_end_request;

#define FCGI_STATUS_REQUEST_COMPLETE 0
#define FCGI_STATUS_CANT_MPX_CONN 1
#define FCGI_STATUS_OVERLOADED 2
#define FCGI_STATUS_UNKNOWN_ROLE 3

#endif
