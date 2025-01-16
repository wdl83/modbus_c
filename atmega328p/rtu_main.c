#include <string.h>
// avr-libc
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "rtu_impl.h"
#include "rtu_log.h"
#include "rtu_memory_impl.h"

#ifndef EEPROM_ADDR_RTU_ADDR
#error "Please define EEPROM_ADDR_RTU_ADDR"
#endif

/*-----------------------------------------------------------------------------*/
static
void dispatch(modbus_rtu_state_t *state, rtu_memory_impl_t *memory_impl)
{
    /* TODO */
}
/*-----------------------------------------------------------------------------*/
__attribute__((noreturn))
void main(void)
{
    uint8_t mcusr = MCUSR;

    rtu_memory_impl_t memory_impl;
    modbus_rtu_state_t state;

    rtu_memory_impl_clear(&memory_impl);
    rtu_memory_impl_init(&memory_impl);
    TLOG_INIT(memory_impl.tlog, TLOG_SIZE);

    RTU_LOG_DBG8("MCUSR", mcusr);

    memory_impl.priv.self_addr =
        eeprom_read_byte((const uint8_t *)EEPROM_ADDR_RTU_ADDR);

    modbus_rtu_impl(
        &state,
        NULL /* suspend */, NULL /* resume */,
        rtu_memory_impl_pdu_cb,
        (uintptr_t)&memory_impl);

    /* set SMCR SE (Sleep Enable bit) */
    sleep_enable();

    for(;;)
    {
        cli(); // disable interrupts
        modbus_rtu_event(&state);
        const bool is_idle = modbus_rtu_idle(&state);
        if(is_idle) dispatch(&state, &memory_impl);
        sei(); // enabled interrupts
        sleep_cpu();
    }
}
