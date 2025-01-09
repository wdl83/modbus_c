#pragma once

#include <stddef.h>
#include <stdint.h>

#include "check.h"
#include "rtu.h"
#include "rtu_memory.h"
#include "tty.h"


typedef struct rtu_memory_impl
{
    rtu_memory_header_t header;
    modbus_rtu_addr_t self_addr;
    char tlog[TLOG_SIZE];
} rtu_memory_impl_t;


void rtu_memory_impl_clear(rtu_memory_impl_t *);
void rtu_memory_impl_init(rtu_memory_impl_t *);

uint8_t *rtu_memory_impl_pdu_cb(
    modbus_rtu_state_t *state,
    modbus_rtu_addr_t addr,
    modbus_rtu_fcode_t fcode,
    const uint8_t *begin, const uint8_t *end, const uint8_t *curr,
    uint8_t *dst_begin, const uint8_t *const dst_end,
    uintptr_t user_data);

int calc_1t5_us(speed_t rate);
int calc_3t5_us(speed_t rate);
// time required to transfer payload (size)
int calc_tmin_ms(speed_t, size_t size);

void modbus_rtu_run(
    tty_dev_t *dev,
    speed_t rate,
    int timeout_1t5_us, int timeout_3t5_us,
    modbus_rtu_pdu_cb_t pdu_cb, uintptr_t user_data,
    struct pollfd *user_event);
