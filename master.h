#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rtu.h"

#define INVALID_PARAM                                                       -255
#define RTU_REPLY_INVALID_SIZE                                                -1
#define RTU_REPLY_INVALID_CRC                                                  1

/* MISC ----------------------------------------------------------------------*/

/* CRC will be implaced into 2 last bytes of the range [end - 2 | end - 1] */
void *implace_crc(void *adu, size_t adu_size);
const void *valid_crc(const void *adu, size_t adu_size);

/* GENERIC -------------------------------------------------------------------*/

typedef struct __attribute__((packed))
{
    modbus_rtu_addr_t addr;
    modbus_rtu_fcode_t fcode;
    modbus_rtu_mem_addr_t mem_addr;
    modbus_rtu_count_t count;
    modbus_rtu_crc_t crc;
} modbus_rtu_mem_access_request_t;

/* FCODE_RD_COILS ------------------------------------------------------------*/

typedef modbus_rtu_mem_access_request_t modbus_rtu_rd_coils_request_t;

char *request_rd_coils(
    modbus_rtu_addr_t,
    modbus_rtu_mem_addr_t, uint8_t count,
    char *dst, size_t max_size);

/* FCODE_RD_HOLDING_REGISTERS ------------------------------------------------*/

typedef modbus_rtu_mem_access_request_t modbus_rtu_rd_holding_registers_request_t;

char *request_rd_holding_registers(
    modbus_rtu_addr_t,
    modbus_rtu_mem_addr_t, uint8_t count,
    char *dst, size_t max_size);

typedef struct __attribute__((packed))
{
    modbus_rtu_addr_t addr;
    modbus_rtu_fcode_t fcode;
    uint8_t count;
} modbus_rtu_rd_holding_registers_reply_header_t;

/* FCODE_WR_COIL -------------------------------------------------------------*/

char *request_wr_coil(
    modbus_rtu_addr_t,
    modbus_rtu_mem_addr_t, uint8_t data,
    char *dst, size_t max_size);

/* FCODE_WR_REGISTER ---------------------------------------------------------*/

char *request_wr_register(
    modbus_rtu_addr_t,
    modbus_rtu_mem_addr_t, uint16_t data,
    char *dst, size_t max_size);

/* FCODE_WR_REGISTERs --------------------------------------------------------*/

char *request_wr_registers(
    modbus_rtu_addr_t,
    modbus_rtu_mem_addr_t, const uint16_t *data, uint8_t count,
    char *dst, size_t max_size);

/* FCODE_WR_BYTES ------------------------------------------------------------*/

typedef struct __attribute__((packed))
{
    modbus_rtu_addr_t addr;
    modbus_rtu_fcode_t fcode;
    modbus_rtu_mem_addr_t mem_addr;
    uint8_t count;
} modbus_rtu_wr_bytes_request_header_t;

char *request_wr_bytes(
    modbus_rtu_addr_t,
    modbus_rtu_mem_addr_t, const uint8_t *data, uint8_t count,
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

/* FCODE_RD_BYTES ------------------------------------------------------------*/

typedef struct __attribute__((packed))
{
    modbus_rtu_addr_t addr;
    modbus_rtu_fcode_t fcode;
    modbus_rtu_mem_addr_t mem_addr;
    uint8_t count;
    modbus_rtu_crc_t crc;
} modbus_rtu_rd_bytes_request_t;

char *request_rd_bytes(
    modbus_rtu_addr_t,
    modbus_rtu_mem_addr_t, uint8_t count,
    char *dst, size_t max_size);

typedef struct __attribute__((packed))
{
    modbus_rtu_addr_t addr;
    modbus_rtu_fcode_t fcode;
    modbus_rtu_mem_addr_t mem_addr;
    uint8_t count;
} modbus_rtu_rd_bytes_reply_header_t;

const modbus_rtu_rd_bytes_reply_header_t *parse_reply_rd_bytes(
    const char *begin, const char *end);

/* MISC ----------------------------------------------------------------------*/

const char *find_ecode(const char *begin, const char *end);
