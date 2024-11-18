#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <drv/tlog.h>

#include "crc.h"
#include "rtu.h"

typedef modbus_rtu_addr_t addr_t;
typedef modbus_rtu_crc_t crc_t;
typedef modbus_rtu_fcode_t fcode_t;
typedef modbus_rtu_state_t state_t;

#define IS_CURR_INIT(status) (RTU_STATE_INIT == (status.bits.curr))
#define IS_CURR_IDLE(status) (RTU_STATE_IDLE == (status.bits.curr))
#define IS_CURR_SOF(status) (RTU_STATE_SOF == (status.bits.curr))
#define IS_CURR_RECV(status) (RTU_STATE_RECV == (status.bits.curr))
#define IS_CURR_EOF(status) (RTU_STATE_EOF == (status.bits.curr))
#define IS_CURR_BUSY(status) (RTU_STATE_BUSY == (status.bits.curr))

#define IS_PREV_INIT(status) (RTU_STATE_INIT == (status.bits.prev))
#define IS_PREV_IDLE(status) (RTU_STATE_IDLE == (status.bits.prev))
#define IS_PREV_SOF(status) (RTU_STATE_SOF == (status.bits.prev))
#define IS_PREV_RECV(status) (RTU_STATE_RECV == (status.bits.prev))
#define IS_PREV_EOF(status) (RTU_STATE_EOF == (status.bits.prev))
#define IS_PREV_BUSY(status) (RTU_STATE_BUSY == (status.bits.prev))

#define IS_ERR(status) (status.bits.error)

static
void reset_rxbuf(state_t *state)
{
    memset(state->rxbuf, 0, sizeof(state->rxbuf));
    state->rxbuf_curr = state->rxbuf;
}

static
void reset_txbuf(state_t *state)
{
    memset(state->txbuf, 0, sizeof(state->txbuf));
    state->txbuf_curr = state->txbuf;
}

static
void timer_silent_interval_cb(state_t *state)
{
    if(IS_CURR_INIT(state->status))
    {
        /* INIT -> IDLE happens on start/restart */
        RTU_STATE_UPDATE(state->status, RTU_STATE_IDLE);
        (*state->timer_stop)(state);
    }
    else if(IS_PREV_RECV(state->status) && IS_CURR_EOF(state->status))
    {
        /* confirmed End of Frame */
        RTU_STATE_UPDATE(state->status, RTU_STATE_IDLE);
        (*state->timer_stop)(state);
    }
    else
    {
        TLOG_XPRINT8("SIE", state->status.value);
        RTU_STATE_ERROR(state->status);
    }
}

static
void timer_inter_frame_timeout_cb(state_t *state)
{
     /* last character of ADU received, silent interval started */
    if(IS_CURR_RECV(state->status))
    {
        /* possible End of Frame detected, already 1,5t elapsed
         * should wait at least 3,5t (in total) to confirm
         * switch timer to 3,5t and wait additional 3,5t (~5t in total) */
        RTU_STATE_UPDATE(state->status, RTU_STATE_EOF);
        (*state->timer_stop)(state);
        state->timer_cb = timer_silent_interval_cb;
        (*state->timer_start_3t5)(state);
    }
    else
    {
        TLOG_XPRINT8("IFE", state->status.value);
        RTU_STATE_ERROR(state->status);
    }
}

static
void rxbuf_append(state_t *state, uint8_t data)
{
    if(state->rxbuf + RXBUF_CAPACITY > state->rxbuf_curr)
    {
        *(state->rxbuf_curr) = data;
        ++(state->rxbuf_curr);
    }
    else
    {
        TLOG_XPRINT8("RAE", state->status.value);
        RTU_STATE_ERROR(state->status);
    }
}

static
void serial_recv_cb(state_t *state, uint8_t data)
{
    if(IS_CURR_IDLE(state->status))
    {
        /* 1st character - SOF detected
         * switch timer from 3,5t to 1,5t */
        RTU_STATE_UPDATE(state->status, RTU_STATE_SOF);
        rxbuf_append(state, data);
        state->timer_cb = timer_inter_frame_timeout_cb;
        (*state->timer_start_1t5)(state);
    }
    else if(IS_CURR_SOF(state->status) || IS_CURR_RECV(state->status))
    {
        /* 2nd, 3rd, ..., Nth character received */
        RTU_STATE_UPDATE(state->status, RTU_STATE_RECV);
        rxbuf_append(state, data);
        (*state->timer_reset)(state);
    }
    else
    {
        TLOG_XPRINT8("SRE", state->status.value);
        RTU_STATE_ERROR(state->status);
    }
}

static
void serial_sent_cb(state_t *state)
{
    if(IS_CURR_BUSY(state->status))
    {
        //TLOG_XPRINT16("SLEN", (uint16_t)(state->txbuf_curr - state->txbuf));
        state->txbuf_curr = state->txbuf;
        RTU_STATE_UPDATE(state->status, RTU_STATE_INIT);
    }
    else
    {
        TLOG_XPRINT8("SSE", state->status.value);
        RTU_STATE_ERROR(state->status);
    }
}

static
void serial_recv_err_cb(state_t *state, uint8_t data)
{
    (void)data;
    /* transmission error */
    ++state->stats.serial_recv_err_cntr;
    RTU_STATE_ERROR(state->status);
}

static
bool adu_check(state_t *state, const uint8_t *begin, const uint8_t *end)
{
    if(ADU_MIN_SIZE > end - begin) return false;

    const uint8_t crc_high = *(end - 1);
    const uint8_t crc_low = *(end - 2);
    const crc_t crc_received = crc_high << 8 | crc_low;
    const crc_t crc_calculated = modbus_rtu_calc_crc(begin, end - 2);

    if(crc_received != crc_calculated)
    {
        //TLOG_XPRINT16("rCRC", crc_received);
        //TLOG_XPRINT16("cCRC", crc_calculated);
        ++state->stats.crc_err_cntr;
        return false;
    }
    return true;
}

static
void adu_process(state_t *state)
{
    if(adu_check(state, state->rxbuf, state->rxbuf_curr))
    {
        const addr_t addr =  state->rxbuf[0];
        const fcode_t fcode = state->rxbuf[1];
        uint8_t *src_begin = state->rxbuf;
        uint8_t *src_curr = state->rxbuf + sizeof(addr_t) + sizeof(fcode_t);
        uint8_t *src_end = state->rxbuf_curr - sizeof(crc_t);
        uint8_t *dst_begin = state->txbuf;
        uint8_t *dst_end = state->txbuf + TXBUF_CAPACITY - sizeof(crc_t);

        state->txbuf_curr =
            (*state->pdu_cb)(
                state,
                addr,
                fcode,
                src_begin, src_end,
                src_curr,
                dst_begin, dst_end,
                state->user_data);

        state->rxbuf_curr = state->rxbuf;

        if(state->txbuf_curr != dst_begin)
        {
            const crc_t crc = modbus_rtu_calc_crc(dst_begin, state->txbuf_curr);
            const uint8_t crc_high = (crc & UINT16_C(0xFF00)) >> 8;
            const uint8_t crc_low = crc & UINT16_C(0x00FF);

            *(state->txbuf_curr) = (uint8_t)crc_low;
            *(++state->txbuf_curr) = (uint8_t)crc_high;
            ++(state->txbuf_curr);
            RTU_STATE_UPDATE(state->status, RTU_STATE_BUSY);
            state->serial_send(state, serial_sent_cb);
        }
    }
    else
    {
        TLOG_XPRINT8("APE", state->status.value);
        RTU_STATE_ERROR(state->status);
    }
}

void modbus_rtu_init(
    state_t *state,
    modbus_rtu_addr_t addr,
    modbus_rtu_timer_start_t timer_start_1t5,
    modbus_rtu_timer_start_t timer_start_3t5,
    modbus_rtu_timer_stop_t timer_stop,
    modbus_rtu_timer_reset_t timer_reset,
    modbus_rtu_serial_send_t serial_send,
    modbus_rtu_pdu_cb_t pdu_cb,
    modbus_rtu_suspend_cb_t suspend_cb,
    modbus_rtu_resume_cb_t resume_cb,
    uintptr_t user_data)
{
    state->timer_start_1t5 = timer_start_1t5;
    state->timer_start_3t5 = timer_start_3t5;
    state->timer_stop = timer_stop;
    state->timer_reset = timer_reset;
    state->timer_cb = NULL;
    state->serial_recv_cb = serial_recv_cb;
    state->serial_recv_err_cb = serial_recv_err_cb;
    state->serial_send = serial_send;
    state->serial_sent_cb = serial_sent_cb;
    state->pdu_cb = pdu_cb;
    state->suspend_cb = suspend_cb;
    state->resume_cb = resume_cb;
    state->user_data = user_data;
    state->stats.err_cntr = 0;
    state->stats.serial_recv_err_cntr = 0;
    state->stats.crc_err_cntr = 0;
    reset_rxbuf(state);
    reset_txbuf(state);

    modbus_rtu_status_t status = {0};

    status.bits.updated = 1;
    status.bits.prev = RTU_STATE_INIT;
    status.bits.curr = RTU_STATE_INIT;

    state->status = status;
    state->addr = addr;
}

void modbus_rtu_event(state_t *state)
{
    if(!state->status.bits.updated) return;
    else state->status.bits.updated = 0;

    /* interrupts should be disabled until this funtion return */

    if(IS_ERR(state->status))
    {
error:
        ++state->stats.err_cntr;

        TLOG_XPRINT16(
            "ERR",
            ((uint16_t)state->stats.err_cntr << 8)
            | state->stats.serial_recv_err_cntr);

        if(state->stats.crc_err_cntr)
        {
            TLOG_XPRINT8("ERRCRC", state->stats.crc_err_cntr);
        }

        RTU_STATE_UPDATE(state->status, RTU_STATE_INIT);
        state->status.bits.updated = 0;
        state->status.bits.error = 0;
        goto restart;
    }
    else if(IS_CURR_INIT(state->status))
    {
restart:
        reset_rxbuf(state);
        reset_txbuf(state);
        (*state->timer_stop)(state);
        state->timer_cb = timer_silent_interval_cb;
        (*state->timer_start_3t5)(state);
    }
    else if(IS_CURR_IDLE(state->status))
    {
        if(IS_PREV_INIT(state->status))
        {
        }
        else if(IS_PREV_EOF(state->status))
        {

            /* previous response transmission still in progress
             * this case should never happen (BUSY state) */
            if(state->txbuf_curr != state->txbuf)
            {
                TLOG_TP();
                goto error;
            }

            /* confirmed End of Frame - verify CRC and process the ADU */
            adu_process(state);
            if(state->resume_cb) (*state->resume_cb)(state->user_data);
        }
        else
        {
            TLOG_TP();
            goto error;
        }
    }
    else if(IS_CURR_SOF(state->status))
    {
        if(state->suspend_cb) (*state->suspend_cb)(state->user_data);

        if(!IS_PREV_IDLE(state->status))
        {
            TLOG_TP();
            goto error;
        }
    }
    else if(IS_CURR_RECV(state->status))
    {
        /* 2nd, 3rd, ..., Nth character received
         * 1.5t timer reset logic moved to recv callback */
    }
    else if(IS_CURR_EOF(state->status))
    {
        if(!IS_PREV_RECV(state->status))
        {
            TLOG_TP();
            goto error;
        }
    }
    else if(IS_CURR_BUSY(state->status))
    {
        /* reply transmission in progress */
    }
    else
    {
        TLOG_TP();
        goto error;
    }
}

modbus_rtu_addr_t modbus_rtu_addr(modbus_rtu_state_t *state)
{
    return state->addr;
}

bool modbus_rtu_idle(modbus_rtu_state_t *state)
{
    return IS_CURR_IDLE(state->status);
}
