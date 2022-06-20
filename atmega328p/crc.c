#include <stddef.h>

#include <util/crc16.h>

#include <modbus_c/crc.h>

uint16_t modbus_rtu_calc_crc(const uint8_t *begin, const uint8_t *end)
{
    if(NULL == begin || NULL == end) return UINT16_C(0xFFFF);

    /* Modbus V1.02:
     * CRC16 polynomial: x^16 + x^15 + x2 + 1
     * 0xA001 == b1010 0000 000 0001
     * initial CRC16 value 0xFFFF */
    uint16_t crc16 = UINT16_C(0xFFFF);

    while(begin != end) crc16 = _crc16_update(crc16, *begin++);
    return crc16;
}
