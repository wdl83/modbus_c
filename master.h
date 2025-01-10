#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rtu.h"

#define INVALID_PARAM                                                       -255
#define RTU_REPLY_INVALID_SIZE                                                -1
#define RTU_REPLY_INVALID_CRC                                                  1

/* CRC will be implaced into 2 last bytes of the range [end - 2 | end - 1] */
void *implace_crc(void *adu, size_t adu_size);

char *request_rd_coils(
    modbus_rtu_addr_t,
    uint16_t mem_addr, uint8_t count,
    char *dst, size_t max_size);

char *request_rd_holding_registers(
    modbus_rtu_addr_t,
    uint16_t mem_addr, uint8_t count,
    char *dst, size_t max_size);

char *request_wr_coil(
    modbus_rtu_addr_t,
    uint16_t mem_addr, uint8_t data,
    char *dst, size_t max_size);

char *request_wr_register(
    modbus_rtu_addr_t,
    uint16_t mem_addr, uint16_t data,
    char *dst, size_t max_size);

char *request_wr_registers(
    modbus_rtu_addr_t,
    uint16_t mem_addr, const uint16_t *data, uint8_t count,
    char *dst, size_t max_size);

typedef struct __attribute__((packed))
{
    modbus_rtu_addr_t addr;
    modbus_rtu_fcode_t fcode;
    modbus_rtu_mem_addr_t mem_addr;
    uint8_t count;
    uint8_t data[];
} modbus_rtu_wr_bytes_request_header_t;

/* usage:
 *    struct
 *    {
 *        modbus_rtu_wr_bytes_request_header_t header;
 *        char data[DATA_SIZE];
 *        modbus_rtu_crc_t crc;
 *    } req = {.header = {...}, ...};
 */

char *request_wr_bytes(
    modbus_rtu_addr_t,
    uint16_t mem_addr, const uint8_t *data, uint8_t count,
    char *dst, size_t max_size);

typedef struct __attribute__((packed))
{
    modbus_rtu_addr_t addr;
    modbus_rtu_fcode_t fcode;
    modbus_rtu_mem_addr_t mem_addr;
    uint8_t count;
    modbus_rtu_crc_t crc;
} modbus_rtu_wr_bytes_reply_t;

const modbus_rtu_wr_bytes_reply_t *parse_reply_wr_bytes(
    const char *begin, const char *end);

char *request_rd_bytes(
    modbus_rtu_addr_t,
    uint16_t mem_addr, uint8_t count,
    char *dst, size_t max_size);

typedef struct __attribute__((packed))
{
    modbus_rtu_addr_t addr;
    modbus_rtu_fcode_t fcode;
    modbus_rtu_mem_addr_t mem_addr;
    uint8_t count;
    modbus_rtu_crc_t crc;
} modbus_rtu_rd_bytes_request_t;

int check_crc(const char *begin, const char *end);
const char *find_ecode(const char *begin, const char *end);
