#include <stdint.h>
#include <string.h>

#include "crc.h"
#include "master.h"
#include "rtu.h"
#include "log.h"


typedef modbus_rtu_addr_t addr_t;
typedef modbus_rtu_fcode_t fcode_t;
typedef modbus_rtu_ecode_t ecode_t;
typedef modbus_rtu_mem_addr_t mem_addr_t;
typedef modbus_rtu_count_t count_t;
typedef modbus_rtu_data16_t data16_t;
typedef modbus_rtu_crc_t crc_t;

static
char *implace_crc_impl(char *const begin, char *const end, const size_t max_size)
{
    if((size_t)(end - begin) + sizeof(crc_t) > max_size) return NULL;

    const crc_t crc = modbus_rtu_calc_crc((const uint8_t *)begin, (const uint8_t *)end);
    char *curr = end;

    *curr++ = crc.low;
    *curr++ = crc.high;
    return curr;
}

void *implace_crc(void *const adu, size_t adu_size)
{
    char *begin = (char *)adu;
    char *end = begin + adu_size;
    return implace_crc_impl(begin, end - sizeof(crc_t), adu_size);
}

const char *valid_crc_impl(const char *const begin, const char *const end)
{
    if((size_t)(end - begin) <= sizeof(crc_t)) return NULL;

    const uint8_t *const payload_begin = (const uint8_t *)begin;
    const uint8_t *const payload_end = (const uint8_t *)(end - sizeof(crc_t));

    const crc_t crc = modbus_rtu_calc_crc(payload_begin, payload_end);
    const uint8_t crc_low = *(end - 2);
    const uint8_t crc_high = *(end - 1);

    if(crc_low != crc.low || crc_high != crc.high) return NULL;
    return end - 2;
}

const void *valid_crc(const void *const adu, const size_t adu_size)
{
    char *begin = (char *)adu;
    char *end = begin + adu_size;
    return valid_crc_impl(begin, end);
}

char *make_request_rd_coils(
    const addr_t slave_addr,
    const mem_addr_t mem_addr, const uint8_t count,
    char *const dst, const size_t max_size)
{
    if(!dst) return NULL;
    if(UINT8_C(0x7D) < count) return NULL;

    const char req[] =
    {
        slave_addr, FCODE_RD_COILS,
        mem_addr.high, mem_addr.low,
        UINT8_C(0), count
    };

    if(sizeof(req) > max_size) return NULL;
    memcpy(dst, req, sizeof(req));
    return implace_crc_impl(dst, dst + sizeof(req), max_size);
}

char *make_request_rd_holding_registers(
    const addr_t slave_addr,
    const mem_addr_t mem_addr, const count_t count,
    char *const dst, const size_t max_size)
{
    if(!dst) return NULL;
    if(UINT8_C(0x7D) < COUNT_TO_WORD(count)) return NULL;
    if(sizeof(modbus_rtu_rd_holding_registers_request_t) > max_size) return NULL;

    modbus_rtu_rd_holding_registers_request_t req =
    {
        .addr = slave_addr,
        .fcode = FCODE_RD_HOLDING_REGISTERS,
        .mem_addr = mem_addr,
        .count = count
    };

    if(!implace_crc(&req, sizeof(req))) return NULL;
    memcpy(dst, &req, sizeof(req));
    return dst + sizeof(req);
}

const modbus_rtu_rd_holding_registers_reply_t *parse_reply_rd_holding_registers(
    const void *adu, size_t adu_size)
{
    const size_t expected_min_size =
        sizeof(modbus_rtu_rd_holding_registers_reply_header_t)
        + sizeof(crc_t);

    if(expected_min_size > adu_size) return NULL;

    const modbus_rtu_rd_holding_registers_reply_t *reply = adu;

    if(expected_min_size + reply->header.byte_count != adu_size) return NULL;
    if(!valid_crc(adu, adu_size)) return NULL;
    return reply;
}

char *make_request_wr_coil(
    const addr_t slave_addr,
    const mem_addr_t mem_addr, const uint8_t data,
    char *const dst, const size_t max_size)
{
    if(!dst) return NULL;
    if(data != UINT8_C(0x00) && data != UINT8_C(0xFF)) return NULL;

    const char req[] =
    {
        slave_addr, FCODE_WR_COIL,
        mem_addr.high, mem_addr.low,
        data, UINT8_C(0)
    };

    if(sizeof(req) > max_size) return NULL; // provided user buffer too small
    memcpy(dst, req, sizeof(req));
    return implace_crc_impl(dst, dst + sizeof(req), max_size);
}

char *make_request_wr_register(
    const addr_t slave_addr,
    const mem_addr_t mem_addr, const uint16_t data,
    char *const dst, const size_t max_size)
{
    const char req[] =
    {
        slave_addr, FCODE_WR_REGISTER,
        mem_addr.high, mem_addr.low,
        HIGH_BYTE(data), LOW_BYTE(data)
    };

    if(sizeof(req) > max_size) return NULL;
    memcpy(dst, req, sizeof(req));
    return implace_crc_impl(dst, dst + sizeof(req), max_size);
}

char *make_request_wr_registers(
    addr_t slave_addr,
    mem_addr_t mem_addr, const data16_t *const data, const count_t count,
    char *const dst, const size_t max_size)
{
    if(!data || !dst) return NULL;
    if(UINT8_C(0x7B) < COUNT_TO_WORD(count)) return NULL;

    const size_t data_size = COUNT_TO_WORD(count) * sizeof(data16_t);
    const size_t expected_size =
        sizeof(modbus_rtu_wr_registers_request_header_t)
        + data_size + sizeof(crc_t);

    if(expected_size > max_size) return NULL;

    modbus_rtu_wr_registers_request_header_t req_header =
    {
        .addr = slave_addr,
        .fcode = FCODE_WR_REGISTERS,
        .mem_addr = mem_addr,
        .count =  count,
        .byte_count = COUNT_TO_WORD(count) << 1,
    };

    char *curr = dst;

    memcpy(curr, &req_header, sizeof(req_header));
    curr += sizeof(req_header);
    memcpy(curr, data, data_size);
    curr += data_size;
    return implace_crc_impl(dst, curr, max_size);
}

const modbus_rtu_wr_registers_reply_t *
parse_reply_wr_registers(const void *const adu, const size_t adu_size)
{
    const size_t expected_size =
        sizeof(addr_t) + sizeof(fcode_t)
        + sizeof(mem_addr_t) + sizeof(count_t)
        + sizeof(crc_t);

    if(expected_size != adu_size) return NULL;
    if(!valid_crc(adu, adu_size)) return NULL;
    return adu;
}

char *make_request_wr_bytes(
    addr_t slave_addr, mem_addr_t mem_addr,
    const uint8_t *const data, const uint8_t count,
    char *const dst, const size_t max_size)
{
    if(!dst || !data) return NULL;
    if(UINT8_C(249) < count) return NULL;

    const char req_header[] =
    {
        slave_addr, FCODE_WR_BYTES,
        mem_addr.high, mem_addr.low,
        count
    };
    char *curr = dst;
    const char *const dst_end = dst + max_size;

    if(sizeof(req_header) > (size_t)(dst_end - curr)) return NULL;
    memcpy(curr, req_header, sizeof(req_header));
    curr += sizeof(req_header);

    if(count > (size_t)(dst_end - curr)) return NULL;
    memcpy(curr, data, count);
    curr += count;
    return implace_crc_impl(dst, curr, max_size);
}

const modbus_rtu_wr_bytes_reply_t *
parse_reply_wr_bytes(const void *const adu, const size_t adu_size)
{
    const size_t expected_size =
        sizeof(addr_t) + sizeof(fcode_t)
        + sizeof(mem_addr_t) + sizeof(uint8_t) /* count */
        + sizeof(crc_t);

    if(expected_size != adu_size) return NULL;
    if(!valid_crc(adu, adu_size)) return NULL;
    return adu;
}

char *make_request_rd_bytes(
    const addr_t slave_addr,
    const mem_addr_t mem_addr, const uint8_t count,
    char *const dst, const size_t max_size)
{
    if(!dst) return NULL;
    if(UINT8_C(249) < count) return NULL;

    const char req[] =
    {
        slave_addr, FCODE_RD_BYTES,
        mem_addr.high, mem_addr.low,
        count
    };

    if(sizeof(req) > max_size) return NULL;
    memcpy(dst, req, sizeof(req));
    return implace_crc_impl(dst, dst + sizeof(req), max_size);
}

const modbus_rtu_rd_bytes_reply_t *
parse_reply_rd_bytes(const void *adu, const size_t adu_size)
{
    const size_t expected_min_size =
        sizeof(modbus_rtu_rd_bytes_reply_header_t) + sizeof(crc_t);

    if(expected_min_size > adu_size) return NULL;

    const modbus_rtu_rd_bytes_reply_t *reply = adu;

    if(expected_min_size + reply->header.count != adu_size) return NULL;
    if(!valid_crc(adu, adu_size)) return NULL;
    return reply;
}

const char *find_ecode(const char *begin, const char *end)
{
    const size_t size = end - begin;
    const size_t expected_size =
        sizeof(addr_t) + sizeof(fcode_t) + sizeof(ecode_t) + sizeof(crc_t);

    if(size != expected_size) return NULL;
    if(!valid_crc(begin, size)) return NULL;
    return begin + sizeof(addr_t) + sizeof(fcode_t);
}
