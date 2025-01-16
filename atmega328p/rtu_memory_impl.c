#include <stddef.h>
#include <string.h>

#include "rtu_memory_impl.h"


#if !defined(RTU_MEMORY_ADDR) || !defined(RTU_MEMORY_SIZE)
#error "Please define RTU_MEMORY_ADDR and RTU_MEMORY_SIZE"
#endif

void rtu_memory_impl_clear(rtu_memory_impl_t *impl)
{
    memset(impl, 0, sizeof(rtu_memory_impl_t));
}

void rtu_memory_impl_init(rtu_memory_impl_t *impl)
{
    impl->header.addr_begin = RTU_MEMORY_ADDR;
    impl->header.addr_end = RTU_MEMORY_ADDR + RTU_MEMORY_SIZE;
}

uint8_t *rtu_memory_impl_pdu_cb(
    modbus_rtu_state_t *state,
    modbus_rtu_addr_t addr,
    modbus_rtu_fcode_t fcode,
    const uint8_t *begin, const uint8_t *end,
    /* curr == begin + sizeof(addr_t) + sizeof(fcode_t) */
    const uint8_t *curr,
    uint8_t *dst_begin, const uint8_t *const dst_end,
    uintptr_t user_data)
{
    rtu_memory_impl_t *memory_impl = (rtu_memory_impl_t *)user_data;

    if(memory_impl->priv.self_addr != addr) goto exit;

    *dst_begin++ = addr;
    dst_begin =
        rtu_memory_pdu_cb(
            (rtu_memory_t *)&memory_impl->header,
            fcode,
            begin + sizeof(addr), end, curr,
            dst_begin, dst_end);
exit:
    return dst_begin;
}
