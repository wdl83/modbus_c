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
} rtu_master_impl_t;

/* return: fail: NULL, success: bytes + count */
const void *rtu_master_write_bytes(
    rtu_master_impl_t *,
    modbus_rtu_addr_t,
    modbus_rtu_mem_addr_t, uint8_t count, const uint8_t *bytes);

/* return: fail: NULL, success: bytes + count */
void *rtu_master_read_bytes(
    rtu_master_impl_t *,
    modbus_rtu_addr_t,
    modbus_rtu_mem_addr_t, uint8_t count, uint8_t *bytes);
