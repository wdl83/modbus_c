#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include "check.h"
#include "log.h"
#include "rtu_impl.h"
#include "time_util.h"
#include "tty.h"
#include "util.h"

static
void log_tty_debug(tty_dev_t *dev)
{
    if(dev->debug.begin == dev->debug.curr) return;

    char *const begin = dev->debug.begin;
    const char *const end = dev->debug.curr;
    const int len = end - begin - (*(end - 1) == '\n' ? 1 : 0);

    logD("%.*s", len, begin);
    dev->debug.curr = begin;
}

static
const char *rtu_state_str(unsigned int state)
{
    const char *str[] = {"INIT", "IDLE", "SOF", "RECV", "EOF", "BUSY"};
    return length_of(str) > state ? str[state] : "?";
}

typedef struct rtu_impl
{
    tty_dev_t *dev;
    speed_t rate;
    struct
    {
        int timeout_1t5;
        int timeout_3t5;
        int64_t timestamp_us;
        int64_t timeout_us;
        uint64_t start_cntr;
        uint64_t stop_cntr;
        uint64_t reset_cntr;
    } timer;
    modbus_rtu_pdu_cb_t pdu_cb;
    uintptr_t user_data;
} rtu_impl_t;

/* 8 data_bits has 2x character
 * 1.5t: time interval required to transmit 1.5 characters
 * 3.5t: time interval required to transmit 3.5 characters
 *
 * CPS = 2 x (rate / 11), number of characters transmitter during 1 second
 *
 * 1t_us = (10^6 * 11) / (2 x rate)
 *
 * to reduce rounding etc
 * 1.5t_us = 6t_us / 4
 *         = (10^6 x 11 x 6) / (2 x rate x 4)
 *         = (10^6 x 11 x 3) / (4 x rate)
 *         = 8'250'000 / rate
 *
 * 3.5t_us = 7t_us / 2
 *         = (10^6 x 11 x 7) / (2 x rate x 2)
 *         = (10^6 x 11 x 7) / (4 x rate)
 *         = 19'250'000 / rate */

int calc_1t5_us(speed_t rate)
{
    const int bps = tty_bps(rate);

    if(19200 > bps) return 8250000 / bps;
    return 750;
}

int calc_3t5_us(speed_t rate)
{
    const int bps = tty_bps(rate);

    if(19200 > bps) return 19250000 / bps;
    return 1750;

}

static
int calc_timeout_ms(speed_t rate, size_t size)
{
    /* t_s = size / (bps / 11)
     *     = (11 * size) / bps
     *
     * t_ms = 1000 * t_s
     *      = (1000 * 11 * size) / bps */
    const int bps = tty_bps(rate);
    return (size * 11000) / bps;
}

static
void timer_start_1t5(modbus_rtu_state_t *state)
{
    CHECK(state);
    CHECK(state->user_data);
    rtu_impl_t *impl = (rtu_impl_t *)state->user_data;
    CHECK(-1 == impl->timer.timeout_us);
    impl->timer.timestamp_us = timestamp_us();
    impl->timer.timeout_us = impl->timer.timeout_1t5;
    ++impl->timer.start_cntr;
}

static
void timer_start_3t5(modbus_rtu_state_t *state)
{
    CHECK(state);
    CHECK(state->user_data);
    rtu_impl_t *impl = (rtu_impl_t *)state->user_data;
    CHECK(-1 == impl->timer.timeout_us);
    impl->timer.timestamp_us = timestamp_us();
    impl->timer.timeout_us = impl->timer.timeout_3t5;
    ++impl->timer.start_cntr;
}

static
void timer_stop(modbus_rtu_state_t *state)
{
    CHECK(state);
    CHECK(state->user_data);
    rtu_impl_t *impl = (rtu_impl_t *)state->user_data;

    if(-1 == impl->timer.timeout_us) return;

    impl->timer.timeout_us = -1;
    ++impl->timer.stop_cntr;
}

static
void timer_reset(modbus_rtu_state_t *state)
{
    CHECK(state);
    CHECK(state->user_data);
    rtu_impl_t *impl = (rtu_impl_t *)state->user_data;
    CHECK(-1 != impl->timer.timeout_us);
    impl->timer.timestamp_us = timestamp_us();
    ++impl->timer.reset_cntr;
}

static
void send(modbus_rtu_state_t *state, modbus_rtu_serial_sent_cb_t sent_cb)
{
    CHECK(state);
    CHECK(state->user_data);
    rtu_impl_t *impl = (rtu_impl_t *)state->user_data;
    CHECK(impl->dev);
    tty_dev_t *dev = impl->dev;

    const char *const begin = (const char *)state->txbuf;
    const char *const end = (const char *)state->txbuf_curr;
    const ssize_t size = (size_t)(end - begin);
    const int timeout = calc_timeout_ms(impl->rate, size);
    const char *const curr = tty_write( impl->dev, begin, end, timeout, -1);

    log_tty_debug(impl->dev);
    CHECK(end == curr);
    CHECK(sent_cb);
    tty_drain(dev->fd);
    sent_cb(state);
    modbus_rtu_event(state);
}

static
uint8_t *pdu_cb_proxy(
    modbus_rtu_state_t *state,
    modbus_rtu_addr_t addr,
    modbus_rtu_fcode_t fcode,
    const uint8_t *begin, const uint8_t *end,
    const uint8_t *curr,
    uint8_t *dst_begin, const uint8_t *const dst_end,
    uintptr_t user_data)
{
    CHECK(state);
    CHECK(state->user_data);
    rtu_impl_t *impl = (rtu_impl_t *)state->user_data;
    CHECK(impl->pdu_cb);

    return
        impl->pdu_cb(
            state,
            addr, fcode,
            begin, end,
            curr,
            dst_begin, dst_end,
            impl->user_data);
}

void modbus_rtu_run(
    tty_dev_t *dev,
    speed_t rate,
    modbus_rtu_addr_t self_addr,
    modbus_rtu_pdu_cb_t pdu_cb,
    int timeout_1t5, int timeout_3t5,
    uintptr_t user_data,
    int peer_fd)
{
    rtu_impl_t impl =
    {
        .dev = dev,
        .rate = rate,
        .timer =
        {
            .timeout_1t5 = -1 == timeout_1t5 ? calc_1t5_us(rate) : timeout_1t5,
            .timeout_3t5 = -1 == timeout_3t5 ? calc_3t5_us(rate) : timeout_3t5,
            .timestamp_us = timestamp_us(),
            .timeout_us = -1,
            .start_cntr = 0,
            .stop_cntr = 0,
            .reset_cntr = 0
        },
        .pdu_cb = pdu_cb,
        .user_data = user_data
    };

    logD("1.5t %dus, 3.5t %dus", impl.timer.timeout_1t5, impl.timer.timeout_3t5);

    modbus_rtu_state_t state;

    modbus_rtu_init(
        &state,
        self_addr,
        timer_start_1t5, timer_start_3t5, timer_stop, timer_reset,
        send,
        pdu_cb_proxy,
        NULL /* suspend */, NULL /* resume */,
        (uintptr_t)&impl);

    char buf[ADU_CAPACITY];

    for(;;)
    {
        modbus_rtu_event(&state);
        memset(buf, 0, sizeof(buf));

        const char *const end =
            -1 == impl.timer.timeout_us
            ? tty_read(dev, buf, buf + sizeof(buf), -1, peer_fd)
            : tty_read_ll(dev, buf, buf + sizeof(buf), impl.timer.timeout_us);

        log_tty_debug(dev);

        for(const char *begin = buf; begin != end; ++begin)
        {
            // TODO: handle serial errors (serial_recv_err_cb)
            state.serial_recv_cb(&state, *begin);
            modbus_rtu_event(&state);
        }

        if(-1 != impl.timer.timeout_us)
        {
            const int64_t elapsed = timestamp_us() - impl.timer.timestamp_us;

            if(elapsed >= impl.timer.timeout_us)
            {
                logD("timeout %" PRId64 "us", elapsed);
                CHECK(state.timer_cb);
                state.timer_cb(&state);
                modbus_rtu_event(&state);
            }
        }
    }
}
