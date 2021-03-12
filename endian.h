#ifndef ENDIAN_H
#define ENDIAN_H

#include <string.h>

#include "types.h"

static inline u16 read_u16(u8 const *buf) {
    u8 tmp[sizeof(u16)];
    tmp[0] = buf[1];
    tmp[1] = buf[2];
    u16 result;
    memcpy(&result, tmp, sizeof(u16));
    return result;
}

static inline void write_u16(u16 val, void *out) {
    u8 tmp[sizeof(u16)];
    memcpy(tmp, &val, sizeof(u16));
    tmp[0] ^= tmp[1];
    tmp[1] ^= tmp[0];
    tmp[0] ^= tmp[1];
    memcpy(out, tmp, sizeof(u16));
}

static inline void write_u32(u32 val, void *out) {
    u8 tmp[sizeof(u32)];
    memcpy(tmp, &val, sizeof(u32));
    for (int i = 0; i < 2; ++i) {
        tmp[i] ^= tmp[sizeof(u32) - i - 1];
        tmp[sizeof(u32) - i - 1] ^= tmp[i];
        tmp[i] ^= tmp[sizeof(u32) - i - 1];
    }
    memcpy(out, tmp, sizeof(u32));
}

#endif
