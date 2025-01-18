#pragma once

#include <stddef.h>
// avr drv
#include "drv/assert.h"
#include "drv/util.h"
// modbus_c
#include "rtu.h"
#include "rtu_memory.h"


typedef struct
{
    /* begin: private memory (not accessible via rtu_memory_t *) */
    struct
    {
        modbus_rtu_addr_t self_addr;
    } priv;
    /* end: private memory */
    rtu_memory_header_t header;
    union
    {
        struct
        {
            char unused[RTU_MEMORY_SIZE - TLOG_SIZE];
            char tlog[TLOG_SIZE];
        };
        struct
        {
            uint8_t bytes[RTU_MEMORY_SIZE];
        };
    };
} rtu_memory_impl_t;

enum
{
    RTU_MEM_OFFSET =
        sizeof_field(rtu_memory_impl_t, priv)
        + sizeof_field(rtu_memory_impl_t, header)
};

#define VALIDATE_RTU_MEM_OFFSET(field, offset) \
    STATIC_ASSERT_STRUCT_OFFSET( \
        rtu_memory_impl_t, \
        field, \
        sizeof_field(rtu_memory_impl_t, priv) \
        + sizeof_field(rtu_memory_impl_t, header) \
        + offset)

VALIDATE_RTU_MEM_OFFSET(tlog, 824);
VALIDATE_RTU_MEM_OFFSET(bytes, 0);

void rtu_memory_impl_clear(rtu_memory_impl_t *);
void rtu_memory_impl_init(rtu_memory_impl_t *);

uint8_t *rtu_memory_impl_pdu_cb(
    modbus_rtu_state_t *,
    modbus_rtu_addr_t,
    modbus_rtu_fcode_t,
    const uint8_t *begin, const uint8_t *end, const uint8_t *curr,
    uint8_t *dst_begin, const uint8_t *const dst_end,
    uintptr_t user_data);
