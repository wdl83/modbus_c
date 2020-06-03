#pragma once

#include <stddef.h>

#include "rtu.h"

typedef struct
{
    uint16_t addr_begin;
    uint16_t addr_end;

    union
    {
        uint16_t words[0];
        uint8_t bytes[0];
    };
} rtu_memory_t;

uint8_t *rtu_memory_pdu_cb(
    rtu_memory_t *rtu_memory,
    modbus_rtu_fcode_t fcode,
    const uint8_t *begin, const uint8_t *end,
    const uint8_t *curr,
    uint8_t *dst_begin, const uint8_t *const dst_end);
