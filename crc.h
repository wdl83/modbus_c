#pragma once

#include <stdint.h>

uint16_t crc16_update(uint16_t crc, uint8_t data);
uint16_t modbus_rtu_calc_crc(const uint8_t *begin, const uint8_t *end);
