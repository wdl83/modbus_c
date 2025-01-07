#pragma once

#include <stdint.h>

#include "rtu.h"
#include "tty.h"

int calc_1t5_us(speed_t rate);
int calc_3t5_us(speed_t rate);

void modbus_rtu_run(
    tty_dev_t *dev,
    speed_t rate,
    modbus_rtu_addr_t self_addr,
    modbus_rtu_pdu_cb_t pdu_cb,
    int timeout_1t5, int timeout_3t5,
    uintptr_t user_data,
    struct pollfd *user_event);
