#include <stddef.h>
#include <string.h>

#include "rtu_memory.h"
#include "rtu_tlog.h"

#include <drv/util.h>

typedef modbus_rtu_addr_t addr_t;
typedef modbus_rtu_crc_t crc_t;
typedef modbus_rtu_ecode_t ecode_t;
typedef modbus_rtu_fcode_t fcode_t;
typedef modbus_rtu_state_t state_t;

static
uint16_t rd16(const uint8_t *src)
{
    const uint8_t high = *src++;
    const uint8_t low = *src++;
    return high << 8 | low;
}

static
uint8_t *wr16(uint8_t *dst, uint16_t data)
{
    *dst++ = data >> 8;
    *dst++ = data & UINT16_C(0xFF);
    return dst;
}

static
uint8_t *except(fcode_t fcode, ecode_t ecode, uint8_t *reply)
{
    *reply++ = fcode + UINT8_C(0x80);
    *reply++ = ecode;
    return reply;
}

#define RETURN_EXCEPTION_IF(cond, fcode, ecode, reply) \
    do \
    { \
        if((cond)) \
        { \
            RTU_TLOG_TP(); \
            return except((fcode_t)(fcode), (ecode_t)(ecode), (reply)); \
        } \
    } while(0)

#define RETURN_EXCEPTION_IF_NOT(cond, fcode, ecode, reply) \
    RETURN_EXCEPTION_IF(!(cond), (fcode), (ecode), (reply))

#ifndef MODBUS_RTU_MEMORY_RD_HOLDING_REGISTERS_DISABLED
static
uint8_t *read_n16(
    rtu_memory_t *rtu_memory,
    const uint8_t *begin,  const uint8_t *end,
    const uint8_t *curr,
    uint8_t *reply)
{
    (void)begin;

    const uint16_t rtu_mem_begin = rtu_memory->addr_begin;
    const uint16_t rtu_mem_end = rtu_memory->addr_end;
    const uint8_t  request_size = 1 /* fcode */ + 2 /* addr */ + 2 /* num */;

    RETURN_EXCEPTION_IF(
        request_size > end - begin,
        FCODE_WR_BYTES, ECODE_FORMAT_ERROR, reply);

    const uint16_t addr = rd16(curr);
    curr += sizeof(addr);

    RETURN_EXCEPTION_IF_NOT(
        rtu_mem_end > addr && rtu_mem_begin <= addr,
        FCODE_RD_HOLDING_REGISTERS, ECODE_ILLEGAL_DATA_ADDRESS, reply);

    const uint16_t num = rd16(curr);
    curr += sizeof(num);

    RETURN_EXCEPTION_IF_NOT(
        0 < num  && 0x7E > num,
        FCODE_RD_HOLDING_REGISTERS, ECODE_ILLEGAL_DATA_VALUE, reply);

    RETURN_EXCEPTION_IF_NOT(
        rtu_mem_end >= addr + num,
        FCODE_RD_HOLDING_REGISTERS, ECODE_ILLEGAL_DATA_ADDRESS, reply);

#ifdef DEBUG_RTU_MEMORY
    RTU_TLOG_XPRINT16("R", addr);
    RTU_TLOG_XPRINT8("N", (uint8_t)num);
#endif

    /* fcode */
    *reply++ = (uint8_t)FCODE_RD_HOLDING_REGISTERS;
    /* byte count */
    *reply++ = (uint8_t)(num << 1);

    uint16_t addr_begin = addr - rtu_mem_begin;
    const uint16_t  addr_end = addr_begin + num;

    for(; addr_begin != addr_end; ++addr_begin)
    {
        uint16_t data = rtu_memory->bytes[addr_begin];
        reply = wr16(reply, data);
    }

    return reply;
}
#endif /* MODBUS_RTU_MEMORY_RD_HOLDING_REGISTERS_DISABLED */

#ifndef MODBUS_RTU_MEMORY_WR_REGISTER_DISABLED
static
uint8_t *write_16(
    rtu_memory_t *rtu_memory,
    const uint8_t *begin,  const uint8_t *end,
    const uint8_t *curr,
    uint8_t *reply)
{
    const uint16_t rtu_mem_begin = rtu_memory->addr_begin;
    const uint16_t rtu_mem_end = rtu_memory->addr_end;
    const uint8_t  request_size = 1 /* fcode */ + 2 /* addr */ + 2 /* data */;

    RETURN_EXCEPTION_IF(
        request_size > end - begin,
        FCODE_WR_BYTES, ECODE_FORMAT_ERROR, reply);

    uint16_t addr = rd16(curr);
    curr += sizeof(addr);

    RETURN_EXCEPTION_IF_NOT(
        rtu_mem_end > addr,
        FCODE_WR_REGISTER, ECODE_ILLEGAL_DATA_ADDRESS, reply);

    uint16_t data = rd16(curr);
    curr += sizeof(data);

    RETURN_EXCEPTION_IF(
        UINT16_C(0xFF00) & data,
        FCODE_WR_REGISTER, ECODE_ILLEGAL_DATA_VALUE, reply);

#ifdef DEBUG_RTU_MEMORY
    RTU_TLOG_XPRINT16("W", addr);
    RTU_TLOG_XPRINT16("D", data);
#endif

    rtu_memory->bytes[addr - rtu_mem_begin] = data;

    return memcpy(reply, begin, request_size) + request_size;
}
#endif /* MODBUS_RTU_MEMORY_WR_REGISTER_DISABLED */

#ifndef MODBUS_RTU_MEMORY_WR_REGISTERS_DISABLED
static
uint8_t *write_n16(
    rtu_memory_t *rtu_memory,
    const uint8_t *begin,  const uint8_t *end,
    const uint8_t *curr,
    uint8_t *reply)
{
    const uint16_t rtu_mem_begin = rtu_memory->addr_begin;
    const uint16_t rtu_mem_end = rtu_memory->addr_end;
    const uint8_t  request_size = 1 /* fcode */ + 2 /* addr */ + 2 /* num */;

    RETURN_EXCEPTION_IF(
        request_size > end - begin,
        FCODE_WR_BYTES, ECODE_FORMAT_ERROR, reply);

    const uint16_t addr = rd16(curr);
    curr += sizeof(addr);

    RETURN_EXCEPTION_IF_NOT(
        rtu_mem_end > addr,
        FCODE_WR_REGISTERS, ECODE_ILLEGAL_DATA_ADDRESS, reply);

    const uint16_t num = rd16(curr);
    curr += sizeof(num);

    RETURN_EXCEPTION_IF_NOT(
        0 < num  && 0x7C > num,
        FCODE_WR_REGISTERS, ECODE_ILLEGAL_DATA_VALUE, reply);

    const uint8_t byte_count = *curr;
    curr += sizeof(byte_count);

    RETURN_EXCEPTION_IF(
        byte_count != (num << 1),
        FCODE_WR_REGISTERS, ECODE_ILLEGAL_DATA_VALUE, reply);

    RETURN_EXCEPTION_IF(
        byte_count != end - curr,
        FCODE_WR_REGISTERS, ECODE_FORMAT_ERROR, reply);

#ifdef DEBUG_RTU_MEMORY
    RTU_TLOG_XPRINT16("nW", addr);
    RTU_TLOG_XPRINT8("N", (uint8_t)num);
#endif

    uint16_t addr_begin = addr - rtu_mem_begin;
    const uint16_t addr_end = addr_begin + num;

    for(; addr_begin != addr_end; ++addr_begin)
    {
        uint16_t data = rd16(curr);
        curr += sizeof(data);

        RETURN_EXCEPTION_IF(
            UINT16_C(0xFF00) & data,
            FCODE_WR_REGISTERS, ECODE_ILLEGAL_DATA_VALUE, reply);

        rtu_memory->bytes[addr_begin] = data;
    }

    return memcpy(reply, begin, request_size) + request_size;
}
#endif /* MODBUS_RTU_MEMORY_WR_REGISTERS_DISABLED */

static
uint8_t *read_n8(
    rtu_memory_t *rtu_memory,
    const uint8_t *begin,  const uint8_t *end,
    const uint8_t *curr,
    uint8_t *reply)
{
    const uint16_t rtu_mem_begin = rtu_memory->addr_begin;
    const uint16_t rtu_mem_end = rtu_memory->addr_end;
    const uint8_t  request_size = 1 /* fcode */ + 2 /* addr */ + 1 /* num */;

    RETURN_EXCEPTION_IF(
        request_size > end - begin,
        FCODE_WR_BYTES, ECODE_FORMAT_ERROR, reply);

    const uint16_t addr = rd16(curr);
    curr += sizeof(addr);

    RETURN_EXCEPTION_IF_NOT(
        rtu_mem_end > addr && rtu_mem_begin <= addr,
        FCODE_RD_BYTES, ECODE_ILLEGAL_DATA_ADDRESS, reply);

    const uint8_t num = *curr;
    curr += sizeof(num);

    /* max 249 bytes in PDU: 256 - 7 (slave_addr + fcode + addr + num + crc)*/
    RETURN_EXCEPTION_IF_NOT(
        0 < num  && 250 > num,
        FCODE_RD_BYTES, ECODE_ILLEGAL_DATA_VALUE, reply);

    RETURN_EXCEPTION_IF_NOT(
        rtu_mem_end >= addr + num,
        FCODE_RD_BYTES, ECODE_ILLEGAL_DATA_ADDRESS, reply);

#ifdef DEBUG_RTU_MEMORY
    RTU_TLOG_XPRINT16("nR8", addr);
    RTU_TLOG_XPRINT8("N", num);
#endif

    reply = memcpy(reply, begin, request_size);
    /* TODO: check for possible SDCC bug memcpy(...) + request_size  */
    reply += request_size;

    uint16_t addr_begin = addr - rtu_mem_begin;
    const uint16_t  addr_end = addr_begin + num;

    for(; addr_begin != addr_end; ++addr_begin)
    {
        *reply++ = rtu_memory->bytes[addr_begin];
    }

    return reply;
}

static
uint8_t *write_n8(
    rtu_memory_t *rtu_memory,
    const uint8_t *begin,  const uint8_t *end,
    const uint8_t *curr,
    uint8_t *reply)
{
    const uint16_t rtu_mem_begin = rtu_memory->addr_begin;
    const uint16_t rtu_mem_end = rtu_memory->addr_end;
    const uint16_t request_size =  1 /* fcode */ + 2 /* addr */ + 1 /* num */;

    RETURN_EXCEPTION_IF(
        (ptrdiff_t)request_size > end - begin,
        FCODE_WR_BYTES, ECODE_FORMAT_ERROR, reply);

    const uint16_t addr = rd16(curr);
    curr += sizeof(addr);

    RETURN_EXCEPTION_IF_NOT(
        rtu_mem_end > addr,
        FCODE_WR_BYTES, ECODE_ILLEGAL_DATA_ADDRESS, reply);

    const uint8_t num = *curr;
    curr += sizeof(num);

    /* max 249 bytes in PDU: 256 - 7 (slave_addr + fcode + addr + num + crc)*/
    RETURN_EXCEPTION_IF_NOT(
        0 < num && 250 > num,
        FCODE_WR_BYTES, ECODE_ILLEGAL_DATA_VALUE, reply);

    RETURN_EXCEPTION_IF(
        num != end - curr,
        FCODE_WR_BYTES, ECODE_ILLEGAL_DATA_VALUE, reply);

#ifdef DEBUG_RTU_MEMORY
    RTU_TLOG_XPRINT16("nW8", addr);
    RTU_TLOG_XPRINT8("N", (uint8_t)num);
#endif

    uint16_t offset_begin = addr - rtu_mem_begin;
    const uint16_t offset_end = offset_begin + num;

    for(; offset_begin != offset_end; ++offset_begin)
    {
        rtu_memory->bytes[offset_begin] = *curr;
        curr += sizeof(rtu_memory->bytes[offset_begin]);
    }

    return memcpy(reply, begin, request_size) + request_size;
}

uint8_t *rtu_memory_pdu_cb(
    rtu_memory_t *rtu_memory,
    modbus_rtu_fcode_t fcode,
    /* begin points to fcode */
    const uint8_t *begin, const uint8_t *end,
    /* curr == begin + sizeof(fcode_t) */
    const uint8_t *curr,
    uint8_t *dst_begin, const uint8_t *const dst_end)
{
    (void)dst_end;
#ifndef MODBUS_RTU_MEMORY_RD_HOLDING_REGISTERS_DISABLED
    if(FCODE_RD_HOLDING_REGISTERS == fcode)
    {
        dst_begin = read_n16(rtu_memory, begin, end, curr, dst_begin);
        goto exit;
    }
#endif

#ifndef MODBUS_RTU_MEMORY_WR_REGISTER_DISABLED
    if(FCODE_WR_REGISTER == fcode)
    {
        dst_begin = write_16(rtu_memory, begin, end, curr, dst_begin);
        goto exit;
    }
#endif

#ifndef MODBUS_RTU_MEMORY_WR_REGISTERS_DISABLED
    if(FCODE_WR_REGISTERS == fcode)
    {
        dst_begin = write_n16(rtu_memory, begin, end, curr, dst_begin);
        goto exit;
    }
#endif

    if(FCODE_RD_BYTES == fcode)
    {
        dst_begin = read_n8(rtu_memory, begin, end, curr, dst_begin);
        goto exit;
    }

    if(FCODE_WR_BYTES == fcode)
    {
        dst_begin = write_n8(rtu_memory, begin, end, curr, dst_begin);
        goto exit;
    }

    dst_begin = except(fcode, ECODE_ILLEGAL_FUNCTION, dst_begin);
exit:
    return dst_begin;
}
