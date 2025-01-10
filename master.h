#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rtu.h"

#define INVALID_PARAM                                                       -255
#define RTU_REPLY_INVALID_SIZE                                                -1
#define RTU_REPLY_INVALID_CRC                                                  1


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

char *request_wr_bytes(
    modbus_rtu_addr_t,
    uint16_t mem_addr, const uint8_t *data, uint8_t count,
    char *dst, size_t max_size);

typedef struct
{
    modbus_rtu_addr_t addr;
    modbus_rtu_fcode_t fcode;
    struct
    {
        uint8_t high;
        uint8_t low;
    } mem_addr;
    uint8_t count;
    struct
    {
        uint8_t low;
        uint8_t high;
    } crc;
} modbus_rtu_wr_bytes_reply_t;

const modbus_rtu_wr_bytes_reply_t *parse_reply_wr_bytes(
    const char *begin, const char *end);

char *request_rd_bytes(
    modbus_rtu_addr_t,
    uint16_t mem_addr, uint8_t count,
    char *dst, size_t max_size);

int check_crc(const char *begin, const char *end);
const char *find_ecode(const char *begin, const char *end);
