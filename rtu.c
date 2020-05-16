#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <drv/assert.h>
#include <drv/tlog.h>

#include <modbus-c/crc.h>
#include <modbus-c/rtu.h>

typedef modbus_rtu_addr_t addr_t;
typedef modbus_rtu_crc_t crc_t;
typedef modbus_rtu_fcode_t fcode_t;
typedef modbus_rtu_state_t state_t;

#define IS_CURR_INIT(status) (RTU_STATE_INIT == (status.curr))
#define IS_CURR_IDLE(status) (RTU_STATE_IDLE == (status.curr))
#define IS_CURR_SOF(status) (RTU_STATE_SOF == (status.curr))
#define IS_CURR_RECV(status) (RTU_STATE_RECV == (status.curr))
#define IS_CURR_EOF(status) (RTU_STATE_EOF == (status.curr))

#define IS_PREV_INIT(status) (RTU_STATE_INIT == (status.prev))
#define IS_PREV_IDLE(status) (RTU_STATE_IDLE == (status.prev))
#define IS_PREV_SOF(status) (RTU_STATE_SOF == (status.prev))
#define IS_PREV_RECV(status) (RTU_STATE_RECV == (status.prev))
#define IS_PREV_EOF(status) (RTU_STATE_EOF == (status.prev))

#define IS_ERR(status) (status.error)

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
    ASSERT(NULL != state);

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
        RTU_STATE_ERROR(state->status);
        TLOG_TP();
        ASSERT(NULL);
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
        RTU_STATE_ERROR(state->status);
        TLOG_TP();
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
        ASSERT(NULL);
        RTU_STATE_ERROR(state->status);
    }
}

static
void serial_recv_cb(state_t *state, uint8_t data)
{
    ASSERT(NULL != state);

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
        RTU_STATE_ERROR(state->status);
        TLOG_TP();
    }
}

static
void serial_sent_cb(state_t *state)
{
    TLOG_PRINTF("SLEN %02" PRIX8, state->txbuf_curr - state->txbuf);
    state->txbuf_curr = state->txbuf;
}

static
void serial_recv_err_cb(state_t *state, uint8_t data)
{
    /* transmission error */
    RTU_STATE_ERROR(state->status);
    (*state->timer_reset)(state);
    TLOG_TP();
}

static
bool adu_check(const uint8_t *begin, const uint8_t *end)
{
    if(ADU_MIN_SIZE > end - begin) return false;

    const uint8_t crc_high = *(end - 2);
    const uint8_t crc_low = *(end - 1);
    const crc_t crc_received = crc_high << 8 | crc_low;
    const crc_t crc_calculated = modbus_rtu_calc_crc(begin, end - 2);

    if(crc_received != crc_calculated)
    {
        TLOG_PRINTF("CRC %04" PRIX16 " %04" PRIX16, crc_received, crc_calculated);
        return false;
    }
    return true;
}

static
void adu_process(state_t *state)
{
    if(adu_check(state->rxbuf, state->rxbuf_curr))
    {
        const addr_t addr =  state->rxbuf[0];
        const fcode_t fcode = state->rxbuf[1];
        uint8_t *src_begin = state->rxbuf;
        uint8_t *src_curr = state->rxbuf + sizeof(addr_t) + sizeof(fcode_t);
        uint8_t *src_end = state->rxbuf_curr - sizeof(crc_t);
        uint8_t *dst_begin = state->txbuf;
        uint8_t *dst_end = state->txbuf + TXBUF_CAPACITY - sizeof(crc_t);

        /* previous response transmission still in progress */
        if(state->txbuf_curr != state->txbuf) return;

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
            const uint8_t crc_high = (crc >> 8) & 0xFF;
            const uint8_t crc_low = crc & 0xFF;

            *(state->txbuf_curr) = (uint8_t)crc_high;
            *(++state->txbuf_curr) = (uint8_t)crc_low;
            ++(state->txbuf_curr);
            state->serial_send(state, serial_sent_cb);
        }
    }
}

void modbus_rtu_init(
    state_t *state,
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
    state->status =
        (modbus_rtu_status_t)
        {
            .updated = 1,
            .error = 0,
            .prev = RTU_STATE_INIT,
            .curr = RTU_STATE_INIT
        };
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
    reset_rxbuf(state);
    reset_txbuf(state);
}

void modbus_rtu_event(state_t *state)
{
    if(!state->status.updated) return;
    else state->status.updated = 0;

    if(IS_ERR(state->status))
    {
        TLOG_TP();
        RTU_STATE_UPDATE(state->status, RTU_STATE_INIT);
        state->status.updated = 0;
        state->status.error = 0;
        goto restart;
    }
    else if(IS_CURR_INIT(state->status))
    {
restart:
        reset_rxbuf(state);
        reset_rxbuf(state);
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
            /* confirmed End of Frame - verify CRC and process the ADU */
            adu_process(state);
            (*state->resume_cb)(state->user_data);
        }
        else
        {
            ASSERT(NULL);
        }
    }
    else if(IS_CURR_SOF(state->status))
    {
        (*state->suspend_cb)(state->user_data);
        ASSERT(IS_PREV_IDLE(state->status));
    }
    else if(IS_CURR_RECV(state->status))
    {
        /* 2nd, 3rd, ..., Nth character received
         * 1.5t timer reset logic moved to recv callback */
    }
    else if(IS_CURR_EOF(state->status))
    {
        ASSERT(IS_PREV_RECV(state->status));
    }
    else
    {
        ASSERT(NULL);
    }
}

bool modbus_rtu_idle(modbus_rtu_state_t *state)
{
    return IS_CURR_IDLE(state->status);
}
