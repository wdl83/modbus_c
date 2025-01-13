#pragma once

#include <stdint.h>

#include "rtu.h"

typedef struct
{
    modbus_rtu_state_t *rtu_state;
} rtu_impl_t;

void modbus_rtu_impl(
    modbus_rtu_state_t *,
    modbus_rtu_suspend_cb_t,
    modbus_rtu_resume_cb_t,
    modbus_rtu_pdu_cb_t,
    uintptr_t user_data);
