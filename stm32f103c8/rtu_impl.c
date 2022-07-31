#include "rtu_impl.h"

#include <drv/stm32_gpio.h>
#include <drv/stm32_nvic.h>
#include <drv/stm32_rcc.h>
#include <drv/stm32_tim.h>
#include <drv/stm32_usart.h>
#include <drv/usart_async_rx.h>
#include <drv/usart_async_tx.h>

/* 19200bps: t1,5 = 750us, t3,5 = 1750us */
/*----------------------------------------------------------------------------*/
static usart_rx_ctrl_t rx_ctrl_;
static usart_tx_ctrl_t tx_ctrl_;
static uint8_t rxbuf_[1];
static modbus_rtu_state_t *state_;
/*----------------------------------------------------------------------------*/
uint32_t tim2spurious_;

void isrTIM2(void)
{
    TIM_UPDATE_INT_CLEAR(TIM2_BASE);
    if(!TIM_ENABLED(TIM2_BASE))
    {
        ++tim2spurious_;
        return;
    }
    ASSERT(state_);
    state_->timer_cb(state_);
}
/*----------------------------------------------------------------------------*/
void isrUSART1(void)
{
    if(
        USART_RX_INT_ENABLED(USART1_BASE)
        && USART_RX_READY(USART1_BASE)) usart_rx_isr(USART1_BASE, &rx_ctrl_);
    if(
        USART_TX_READY_INT_ENABLED(USART1_BASE)
        && USART_TX_READY(USART1_BASE)
        ||
        USART_TX_COMPLETE_INT_ENABLED(USART1_BASE)
        && USART_TX_COMPLETE(USART1_BASE)) usart_tx_isr(USART1_BASE, &tx_ctrl_);
}
/*----------------------------------------------------------------------------*/
static
void tim_init(void)
{
    ASSERT(!state_);
    /* TIM2 uses APB1 clk as source. 36MHz / 360 = 100kHz (T = 10us) */
    TIM2_CLK_ENABLE();
    TIM_AUTO_RELOAD_PRELOAD_ENABLE(TIM2_BASE);
    TIM_CLK_DIV(TIM2_BASE, UINT16_C(359));
    ISR_ENABLE(isrNoTIM2);
}

static
void tim_start_1t5(modbus_rtu_state_t *state)
{
    ASSERT(state);
    ASSERT(!state_);
    state_ = state;
    TIM_WR_TARGET(TIM2_BASE, UINT16_C(75)); // 75 * 10us == 750us
    TIM_WR_CNTR(TIM2_BASE, UINT16_C(0));
    TIM_UPDATE_INT_ENABLE(TIM2_BASE);
    TIM_UPDATE_INT_CLEAR(TIM2_BASE);
    TIM_ENABLE(TIM2_BASE);
}

static
void tim_start_3t5(modbus_rtu_state_t *state)
{
    ASSERT(state);
    ASSERT(!state_);
    state_ = state;
    TIM_WR_TARGET(TIM2_BASE, UINT16_C(175)); // 175 * 10us == 1750us
    TIM_WR_CNTR(TIM2_BASE, UINT16_C(0));
    TIM_UPDATE_INT_ENABLE(TIM2_BASE);
    TIM_UPDATE_INT_CLEAR(TIM2_BASE);
    TIM_ENABLE(TIM2_BASE);
}

static
void tim_stop(modbus_rtu_state_t *state)
{
    (void)state;
    TIM_DISABLE(TIM2_BASE);
    TIM_UPDATE_INT_DISABLE(TIM2_BASE);
    TIM_UPDATE_INT_CLEAR(TIM2_BASE);
    state_ = NULL;
}

static
void tim_reset(modbus_rtu_state_t *state)
{
    (void)state;
    TIM_DISABLE(TIM2_BASE);
    TIM_WR_CNTR(TIM2_BASE, 0);
    TIM_UPDATE_INT_CLEAR(TIM2_BASE);
    TIM_ENABLE(TIM2_BASE);
}

static
void usart_init(void)
{
    PORTA_CLK_ENABLE();
    USART1_CLK_ENABLE();
    /* PoR: alternate functions are not active on I/O ports
     * enable USART TX/RX
     *
     * 9.1.4 Alternate functions (AF)
     * "For alternate function inputs, the port must be configured in Input mode
     * (floating, pull-up or pull-down) and the input pin must be driven externally */
    GPIO_CFG(GPIOA_BASE, 9, CFN_OUT_ALT_PUSH_PULL, MODE_50MHz);
    GPIO_CFG(GPIOA_BASE, 10, CFN_IN_PULL_UP_DOWN, MODE_INPUT);
    GPIO_PULL_UP(GPIOA_BASE, 10);

    /* USART1 uses APB2 clk as source */
    USART_ENABLE(USART1_BASE);
    USART_PARITY_EVEN(USART1_BASE);
    USART_PARITY_ENABLE(USART1_BASE);
    USART_BR(USART1_BASE, CALC_BR(APB2_CLK, UINT32_C(19200)));
    USART_TX_ENABLE(USART1_BASE);
    USART_RX_ENABLE(USART1_BASE);
    ISR_ENABLE(isrNoUSART1);
}

static
void rx_complete(uintptr_t base, usart_rx_ctrl_t *ctrl)
{
    ASSERT(ctrl);
    modbus_rtu_state_t *state = (modbus_rtu_state_t *)(ctrl->user_data);

    if(!ctrl->flags.errors.fopn)
    {
        (*state->serial_recv_cb)(state, *ctrl->begin);
    }
    else
    {
        {
            /* RX queue flushing in case of reception errors
             * TODO: verify if this is required */
            while(USART_RX_READY(USART1_BASE)) (void)USART_RD(USART1_BASE);
        }
        (*state->serial_recv_err_cb)(state, *ctrl->begin);
    }

    rx_ctrl_.begin = rxbuf_;
    rx_ctrl_.next = rx_ctrl_.begin;
    rx_ctrl_.flags.value = 0;
    usart_async_recv(USART1_BASE, &rx_ctrl_);
}

void tx_complete(uintptr_t user_data, usart_tx_ctrl_t *ctrl)
{
    ASSERT(ctrl);
    ASSERT(ctrl->user_data);
    modbus_rtu_state_t *state = (modbus_rtu_state_t *)(ctrl->user_data);
    (*state->serial_sent_cb)(state);
}

void serial_send(modbus_rtu_state_t *state, modbus_rtu_serial_sent_cb_t sent_cb)
{
    (void)sent_cb;
    tx_ctrl_.begin = state->txbuf;
    tx_ctrl_.end = state->txbuf_curr;
    tx_ctrl_.complete_cb = tx_complete;
    tx_ctrl_.user_data =  (uintptr_t)state;
    usart_async_send(USART1_BASE, &tx_ctrl_);
}

void modbus_rtu_impl(
    modbus_rtu_state_t *state,
    modbus_rtu_addr_t addr,
    modbus_rtu_suspend_cb_t suspend_cb,
    modbus_rtu_resume_cb_t resume_cb,
    modbus_rtu_pdu_cb_t pdu_cb,
    uintptr_t user_data)
{
    usart_init();
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

    rx_ctrl_.begin = rxbuf_;
    rx_ctrl_.end = rxbuf_ + sizeof(rxbuf_);
    rx_ctrl_.next = rx_ctrl_.begin;
    rx_ctrl_.complete_cb = rx_complete;
    rx_ctrl_.user_data = (uintptr_t)state;

    usart_async_recv(USART1_BASE, &rx_ctrl_);
}
