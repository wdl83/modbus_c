#pragma once

#include <stdint.h>

#include "rtu.h"


modbus_rtu_crc_t crc16_update(modbus_rtu_crc_t, uint8_t data);
modbus_rtu_crc_t modbus_rtu_calc_crc(const uint8_t *begin, const uint8_t *end);
