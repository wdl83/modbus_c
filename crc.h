#pragma once

#include <stdint.h>

uint16_t modbus_rtu_calc_crc(const uint8_t *begin, const uint8_t *end);
