#include <stddef.h>
#include <stdint.h>

#include <drv/check.h>

#include <modbus_c/crc.h>

uint16_t crc16_update(uint16_t crc, uint8_t data)
{
    /* source: AVR GCC crc16 headers
     * Polynomial: x^16 + x^15 + x^2 + 1 (0xa001) */
    crc ^= data;

    for(int8_t i = 0; i < 8; ++i)
    {
        crc = crc & 1 ?  (crc >> 1) ^ 0xA001 : crc >> 1;
    }

    return crc;
}

uint16_t modbus_rtu_calc_crc(const uint8_t *begin, const uint8_t *end)
{
    /* Modbus V1.02:
     * CRC16 polynomial: x^16 + x^15 + x2 + 1
     * 0xA001 == b1010 0000 000 0001
     * initial CRC16 value 0xFFFF */
    uint16_t crc16 = UINT16_C(0xFFFF);

    while(begin != end) crc16 = crc16_update(crc16, *begin++);
    return crc16;
}
