#pragma once

#include <stddef.h>
#include <stdint.h>

#include "master.h"
#include "rtu.h"
#include "tty.h"


typedef struct
{
    tty_dev_t *dev;
    speed_t rate;
    // command execution timeout, depends on hardware
    int timeout_exec_ms;
} rtu_master_impl_t;


/* return: fail: NULL, success: data + count */
void *rtu_master_rd_holding_registers(
    rtu_master_impl_t *,
    modbus_rtu_addr_t,
    modbus_rtu_mem_addr_t, modbus_rtu_count_t count, modbus_rtu_data16_t *data);

/* return: fail: NULL, success: data + count */
const void *rtu_master_wr_registers(
    rtu_master_impl_t *,
    modbus_rtu_addr_t,
    modbus_rtu_mem_addr_t, modbus_rtu_count_t count, const modbus_rtu_data16_t *data);

/* return: fail: NULL, success: bytes + count */
const void *rtu_master_wr_bytes(
    rtu_master_impl_t *,
    modbus_rtu_addr_t,
    modbus_rtu_mem_addr_t, uint8_t count, const uint8_t *bytes);

/* return: fail: NULL, success: bytes + count */
void *rtu_master_rd_bytes(
    rtu_master_impl_t *,
    modbus_rtu_addr_t,
    modbus_rtu_mem_addr_t, uint8_t count, uint8_t *bytes);
