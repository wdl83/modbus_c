#include <stdint.h>
#include <string.h>

#include "crc.h"
#include "master.h"
#include "rtu.h"
#include "log.h"

typedef modbus_rtu_addr_t addr_t;
typedef modbus_rtu_fcode_t fcode_t;
typedef modbus_rtu_ecode_t ecode_t;
typedef modbus_rtu_crc_t crc_t;


static
uint8_t low_byte(uint16_t word)
{
    return word & UINT16_C(0xFF);
}

static
uint8_t high_byte(uint16_t word)
{
    return word >> 8;
}

static
uint16_t make_word(uint8_t high, uint8_t low)
{
    return (uint16_t)high << 8 | (uint16_t)low;
}

static
char *implace_crc(char *const begin, char *const end, const size_t max_size)
{
    if((size_t)(end - begin) + sizeof(crc_t) > max_size) return NULL;

    const crc_t crc = modbus_rtu_calc_crc((const uint8_t *)begin, (const uint8_t *)end);
    char *curr = end;

    *curr++ = low_byte(crc);
    *curr++ = high_byte(crc);
    return curr;
}

char *request_rd_coils(
    const addr_t slave_addr,
    const uint16_t mem_addr, const uint8_t count,
    char *const dst, const size_t max_size)
{
    if(!dst) return NULL;
    if(UINT8_C(0x7D) < count) return NULL;

    const char req[] =
    {
        slave_addr, FCODE_RD_COILS,
        high_byte(mem_addr), low_byte(mem_addr),
        UINT8_C(0), count
    };

    if(sizeof(req) > max_size) return NULL;
    memcpy(dst, req, sizeof(req));
    return implace_crc(dst, dst + sizeof(req), max_size);
}

char *request_rd_holding_registers(
    const addr_t slave_addr,
    const uint16_t mem_addr, const uint8_t count,
    char *const dst, const size_t max_size)
{
    if(!dst) return NULL;
    if(UINT8_C(0x7D) < count) return NULL;

    const char req[] =
    {
        slave_addr, FCODE_RD_HOLDING_REGISTERS,
        high_byte(mem_addr), low_byte(mem_addr),
        UINT8_C(0), count
    };

    if(sizeof(req) > max_size) return NULL;
    memcpy(dst, req, sizeof(req));
    return implace_crc(dst, dst + sizeof(req), max_size);
}

char *request_wr_coil(
    const addr_t slave_addr,
    const uint16_t mem_addr, const uint8_t data,
    char *const dst, const size_t max_size)
{
    if(!dst) return NULL;
    if(data != UINT8_C(0x00) && data != UINT8_C(0xFF)) return NULL;

    const char req[] =
    {
        slave_addr, FCODE_WR_COIL,
        high_byte(mem_addr), low_byte(mem_addr),
        data, UINT8_C(0)
    };

    if(sizeof(req) > max_size) return NULL; // provided user buffer too small
    memcpy(dst, req, sizeof(req));
    return implace_crc(dst, dst + sizeof(req), max_size);
}

char *request_wr_register(
    const addr_t slave_addr,
    const uint16_t mem_addr, const uint16_t data,
    char *const dst, const size_t max_size)
{
    const char req[] =
    {
        slave_addr, FCODE_WR_REGISTER,
        high_byte(mem_addr), low_byte(mem_addr),
        high_byte(data), low_byte(data)
    };

    if(sizeof(req) > max_size) return NULL;
    memcpy(dst, req, sizeof(req));
    return implace_crc(dst, dst + sizeof(req), max_size);
}

char *request_wr_registers(
    addr_t slave_addr,
    uint16_t mem_addr, const uint16_t *const data, const uint8_t count,
    char *const dst, const size_t max_size)
{
    if(!data || !dst) return NULL;
    if(count > UINT8_C(0x7B)) return NULL;

    const char req_header[] =
    {
        slave_addr, FCODE_WR_REGISTERS,
        high_byte(mem_addr), low_byte(mem_addr), /* starting address */
        UINT8_C(0), count, /* quantity of registers */
        count << 1, /* byte_count */
    };
    char *curr = dst;
    const char *const dst_end = dst + max_size;

    if(sizeof(req_header) > (size_t)(dst_end - curr)) return NULL;
    memcpy(curr, req_header, sizeof(req_header));
    curr += sizeof(req_header);

    for(
        const uint16_t *i = data, *const end = data + count;
        i != end; ++i)
    {
        if(dst_end == curr) return NULL;
        *curr++ = high_byte(*i);
        if(dst_end == curr) return NULL;
        *curr++ = low_byte(*i);
    }

    return implace_crc(dst, curr, max_size);
}

char *request_wr_bytes(
    addr_t slave_addr, uint16_t mem_addr,
    const uint8_t *const data, const uint8_t count,
    char *const dst, const size_t max_size)
{
    if(!dst || !data) return NULL;
    if(UINT8_C(249) < count) return NULL;

    const char req_header[] =
    {
        slave_addr, FCODE_WR_BYTES,
        high_byte(mem_addr), low_byte(mem_addr),
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
    return implace_crc(dst, curr, max_size);
}

int parse_reply_wr_bytes(
    uint16_t *const mem_addr, uint8_t *const count,
    const char *const begin, const char *const end)
{
    if(!mem_addr || !count) return INVALID_PARAM;

    const size_t expected_size =
        sizeof(addr_t) + sizeof(fcode_t)
        + sizeof(uint16_t) /* mem addr */ + sizeof(uint8_t) /* count */
        + sizeof(crc_t);

    if((size_t)(end - begin) != expected_size) return RTU_REPLY_INVALID_SIZE;
    if(check_crc(begin, end)) return RTU_REPLY_INVALID_CRC;

    const char *data = begin + sizeof(addr_t) + sizeof(fcode_t);
    const uint8_t rep_mem_addr_high = *data++;
    const uint8_t rep_mem_addr_low = *data++;
    const uint8_t rep_count = *data++;

    *mem_addr = make_word(rep_mem_addr_high, rep_mem_addr_low);
    *count = rep_count;
    return 0;
}

char *request_rd_bytes(
    const addr_t slave_addr,
    const uint16_t mem_addr, const uint8_t count,
    char *const dst, const size_t max_size)
{
    if(!dst) return NULL;
    if(UINT8_C(249) < count) return NULL;

    const char req[] =
    {
        slave_addr, FCODE_RD_BYTES,
        high_byte(mem_addr), low_byte(mem_addr),
        count
    };

    if(sizeof(req) > max_size) return NULL;
    memcpy(dst, req, sizeof(req));
    return implace_crc(dst, dst + sizeof(req), max_size);
}

int check_crc(const char *const begin, const char *const end)
{
    if((size_t)(end - begin) <= sizeof(crc_t)) return RTU_REPLY_INVALID_SIZE;

    const uint8_t *const payload_begin = (const uint8_t *)begin;
    const uint8_t *const payload_end = (const uint8_t *)(end - sizeof(crc_t));

    const crc_t crc = modbus_rtu_calc_crc(payload_begin, payload_end);
    const uint8_t crc_low = *(end - 2);
    const uint8_t crc_high = *(end - 1);

    if(crc_low != low_byte(crc) || crc_high != high_byte(crc))
        return RTU_REPLY_INVALID_CRC;
    return 0;
}

const char *find_ecode(const char *begin, const char *end)
{
    const size_t expected_size =
        sizeof(addr_t) + sizeof(fcode_t) + sizeof(ecode_t) + sizeof(crc_t);

    if((size_t)(end - begin) != expected_size) return NULL;
    if(check_crc(begin, end)) return NULL;
    return begin + sizeof(addr_t) + sizeof(fcode_t);
}
