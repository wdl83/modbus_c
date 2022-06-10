#include "rtu_impl.h"

#include <drv/tim4.h>
#include <drv/uart1_async_rx.h>
#include <drv/uart1_async_tx.h>

/* 19200bps:
 *
 * calculating 8-bit timer prescaler value:
 *
 *  t1,5 = 750us / 255 (8-bit timer) ~3us (3us * 255 == 765us)
 *  t3,5 = 1750us / 255 (8-bit timer) ~7us (7us * 255 == 1785us)
 *
 * 8MHz = 8 * 10^6Hz / 128 = 62500Hz  == 16us
 *
 * 750us / 16us = 46,87 ~ 47 x 16us = 752us
 * 1750us / 16us = 109,37 ~ 110 x 16us = 1760us
 * f = f_clk / (2 x N x (OCRA0 + 1) */

static
void tim_init(void)
{
    TIM4_AUTO_RELOAD_PRELOAD_ENABLE();
    TIM4_CLK_DIV_128();
}

static
void tim_cb(uintptr_t user_data)
{
    TIM4_INT_CLEAR();
    modbus_rtu_state_t *state = (modbus_rtu_state_t *)user_data;
    state->timer_cb(state);
}

static
void tim_start_1t5(modbus_rtu_state_t *state)
{
    tim4_cb(tim_cb, (uintptr_t)state);
    TIM4_WR_TOP(47);
    TIM4_WR_CNTR(0);
    TIM4_INT_ENABLE();
    TIM4_INT_CLEAR();
    TIM4_ENABLE();
}

static
void tim_start_3t5(modbus_rtu_state_t *state)
{
    tim4_cb(tim_cb, (uintptr_t)state);
    TIM4_WR_TOP(110);
    TIM4_WR_CNTR(0);
    TIM4_INT_ENABLE();
    TIM4_INT_CLEAR();
    TIM4_ENABLE();
}

static
void tim_stop(modbus_rtu_state_t *state)
{
    (void)state;
    TIM4_DISABLE();
    TIM4_INT_DISABLE();
    TIM4_INT_CLEAR();
    tim4_cb(NULL, 0);
}

static
void tim_reset(modbus_rtu_state_t *state)
{
    (void)state;
    TIM4_DISABLE();
    TIM4_WR_CNTR(0);
    TIM4_INT_CLEAR();
    TIM4_ENABLE();
}

static
void uart_init(void)
{
    UART1_BR(CALC_BR(CPU_CLK, 19200));
    UART1_PARITY_EVEN();
    UART1_RX_ENABLE();
    UART1_TX_ENABLE();
}

static
void uart_rx_recv_cb(uint8_t data, uart_rxflags_t *flags, uintptr_t user_data)
{
    modbus_rtu_state_t *state = (modbus_rtu_state_t *)user_data;

    if(0 == flags->errors.fopn)
    {
        (*state->serial_recv_cb)(state, data);
    }
    else
    {
        {
            /* RX queue flushing in case of reception errors
             * TODO: verify if this is required */
            while(UART1_RX_READY()) (void)UART1_RD();
        }
        (*state->serial_recv_err_cb)(state, data);
    }
}

void uart_tx_complete_cb(uintptr_t user_data)
{
    modbus_rtu_state_t *state = (modbus_rtu_state_t *)user_data;

    (*state->serial_sent_cb)(state);
}

void serial_send(modbus_rtu_state_t *state, modbus_rtu_serial_sent_cb_t sent_cb)
{
    (void)sent_cb;

    uart1_async_send(
        state->txbuf, state->txbuf_curr, uart_tx_complete_cb, (uintptr_t)state);
}

void modbus_rtu_impl(
    modbus_rtu_state_t *state,
    modbus_rtu_addr_t addr,
    modbus_rtu_suspend_cb_t suspend_cb,
    modbus_rtu_resume_cb_t resume_cb,
    modbus_rtu_pdu_cb_t pdu_cb,
    uintptr_t user_data)
{
    uart_init();
    tim_init();

    modbus_rtu_init(
        state,
        addr,
        tim_start_1t5, tim_start_3t5, tim_stop, tim_reset,
        serial_send,
        pdu_cb,
        suspend_cb,
        resume_cb,
        user_data);

    uart1_async_recv_cb(uart_rx_recv_cb, (uintptr_t)(state));
}
