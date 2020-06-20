#pragma once

#include <stdint.h>

#include <modbus-c/rtu.h>

void modbus_rtu_impl(
    modbus_rtu_state_t *,
    modbus_rtu_addr_t,
    modbus_rtu_suspend_cb_t,
    modbus_rtu_resume_cb_t,
    modbus_rtu_pdu_cb_t,
    uintptr_t user_data);
