#include <stddef.h>
/* avr-libc */
#include <util/crc16.h>

#include "crc.h"

modbus_rtu_crc_t modbus_rtu_calc_crc(const uint8_t *begin, const uint8_t *end)
{
    /* Modbus V1.02:
     * CRC16 polynomial: x^16 + x^15 + x2 + 1
     * 0xA001 == b1010 0000 000 0001
     * initial CRC16 value 0xFFFF */

    modbus_rtu_crc_t crc = {.low = UINT8_C(0xFF), .high = UINT8_C(0xFF)};

    if(NULL == begin || NULL == end) return crc;

    while(begin != end) crc = WORD_TO_CRC(_crc16_update(CRC_TO_WORD(crc), *begin++));
    return crc;
}
