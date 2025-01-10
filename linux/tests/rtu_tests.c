#include <assert.h>
#include <bits/pthreadtypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <poll.h>
#include <pthread.h>

#include "check.h"
#include "log.h"
#include "master.h"
#include "pipe.h"
#include "rtu_impl.h"
#include "tty.h"
#include "tty_pair.h"
#include "utest.h"
#include "util.h"

#define DEBUG_SIZE 1024
#define RTU_ADDR UINT8_C(0xAA)

static
void init(tty_dev_t *master, tty_dev_t *slave)
{
    const int debug_size = LOG_LEVEL_DEBUG > current_log_level() ? 0 : DEBUG_SIZE;
    tty_pair_t pair;

    tty_pair_init(&pair);
    tty_pair_create(&pair, TTY_DEFAULT_MULTIPLEXOR, NULL);
    tty_init(master, debug_size);
    tty_init(slave, debug_size);
    tty_adopt(master, pair.master_fd);
    tty_open(slave, pair.slave_path, NULL);
    tty_exclusive_on(master->fd);
    tty_exclusive_on(slave->fd);
    tty_pair_deinit(&pair);
}

static
void config(tty_dev_t *master, tty_dev_t *slave, speed_t rate, parity_t parity)
{
    tty_dev_t *devs[] = {master, slave};
    const stop_bits_t stop_bits = PARITY_none == parity ? STOP_BITS_2 : STOP_BITS_1;

    for(tty_dev_t **pdev = devs; pdev != devs + length_of(devs); ++pdev)
        tty_configure(*pdev, rate, parity, DATA_BITS_8, stop_bits);
}

static
void deinit(tty_dev_t *master, tty_dev_t *slave)
{
    tty_deinit(slave);
    tty_deinit(master);
}

typedef struct rtu_config
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    tty_dev_t *dev;
    speed_t rate;
    modbus_rtu_addr_t self_addr;
    rtu_memory_impl_t memory_impl;
    modbus_rtu_pdu_cb_t pdu_cb;
    int timeout_1t5_us;
    int timeout_3t5_us;
    struct pollfd event;
} rtu_config_t;

static
void *async_run_rtu(void *user_data)
{
    assert(user_data);
    rtu_config_t *config = user_data;

    rtu_memory_impl_clear(&config->memory_impl);
    rtu_memory_impl_init(&config->memory_impl);
    config->memory_impl.self_addr = config->self_addr;

    pthread_cond_signal(&config->cond);

    modbus_rtu_run(
        config->dev,
        config->rate,
        config->timeout_1t5_us, config->timeout_3t5_us,
        config->pdu_cb, (uintptr_t)(&config->memory_impl),
        &config->event);
    return NULL;
}

struct TestFixture
{
    tty_dev_t master;
    tty_dev_t slave;
    pipe_t channel;
    rtu_config_t rtu_config;
    pthread_t runner;
};

UTEST_I_SETUP(TestFixture)
{
    // supports INDEX [0, 7)
    const speed_t rates[] = {B1200, B2400, B4800, B9600, B19200, B57600, B115200};
    const speed_t rate = rates[utest_index];
    struct TestFixture *tf = utest_fixture;

    logI("%d %dbps", (int)utest_index, tty_bps(rate));
    ASSERT_TRUE(tf);

    memset(tf, 0, sizeof(struct TestFixture));

    init(&tf->master, &tf->slave);
    config(&tf->master, &tf->slave, rate, PARITY_none);
    tty_flush(tf->master.fd);
    tty_flush(tf->slave.fd);
    pipe_open(&tf->channel, NULL);
    tf->rtu_config.dev = &tf->slave;
    tf->rtu_config.rate = rate;
    tf->rtu_config.self_addr = RTU_ADDR;
    tf->rtu_config.pdu_cb = rtu_memory_impl_pdu_cb;
    tf->rtu_config.timeout_1t5_us = calc_1t5_us(rate);
    tf->rtu_config.timeout_3t5_us = calc_3t5_us(rate);
    tf->rtu_config.event.fd = tf->channel.reader;
    tf->rtu_config.event.events = POLLIN;

    pthread_mutex_init(&tf->rtu_config.mutex, NULL);
    pthread_mutex_lock(&tf->rtu_config.mutex);

    CHECK_ERRNO(0 == pthread_create(&tf->runner, NULL, async_run_rtu, &tf->rtu_config));

    // wait for RTU thread to start
    pthread_cond_wait(&tf->rtu_config.cond, &tf->rtu_config.mutex);
    pthread_mutex_unlock(&tf->rtu_config.mutex);

    // time required for RTU to transition from INIT to IDLE state
    usleep(tf->rtu_config.timeout_3t5_us);
}

UTEST_I_TEARDOWN(TestFixture)
{
    struct TestFixture *tf = utest_fixture;

    ASSERT_TRUE(tf);

    const char stop[] = "STOP";

    CHECK_ERRNO(-1 != write(tf->channel.writer, stop, sizeof(stop)));
    CHECK_ERRNO(0 == pthread_join(tf->runner, NULL));
    deinit(&tf->master, &tf->slave);
}

UTEST_I(TestFixture, read_bytes_N1, 7)
{
    struct TestFixture *tf = utest_fixture;

    ASSERT_TRUE(tf);

    {
        char req[ADU_CAPACITY];
        memset(req, 0, sizeof(req));

        const char *const req_end =
            request_rd_bytes(tf->rtu_config.self_addr, RTU_ADDR_BASE, 1, req, sizeof(req));

        ASSERT_NE(NULL, req_end);

        const int tx_tmax_ms = 10 + calc_tmin_ms(tf->rtu_config.rate, req_end - req);
        EXPECT_EQ(req_end, tty_write(&tf->master, req, req_end, tx_tmax_ms, NULL));
        tty_logD(&tf->master);
    }

    {
        char rep[ADU_CAPACITY];
        memset(rep, 0, sizeof(rep));

        const int rx_tmax_ms = 10 + calc_tmin_ms(tf->rtu_config.rate, sizeof(rep));
        const char *const rep_end =
            tty_read(&tf->master, rep, rep + length_of(rep) -1, rx_tmax_ms, NULL);

        EXPECT_NE(rep, rep_end);
        tty_logD(&tf->master);

        EXPECT_EQ(0, check_crc(rep, rep_end));
        EXPECT_EQ(NULL, find_ecode(rep,rep_end));
    }
}

UTEST_I(TestFixture, write_bytes_STR, 7)
{
    struct TestFixture *tf = utest_fixture;

    ASSERT_TRUE(tf);

    const char message[] = "!!!hello this is RTU memory!!!";

    {
        char req[ADU_CAPACITY];
        memset(req, 0, sizeof(req));
        const char *const req_end =
            request_wr_bytes(
                tf->rtu_config.self_addr,
                RTU_ADDR_BASE, (const uint8_t *)message, length_of(message),
                req, sizeof(req));

        ASSERT_NE(NULL, req_end);

        const int tx_tmax_ms = 10 + calc_tmin_ms(tf->rtu_config.rate, req_end - req);
        EXPECT_EQ(req_end, tty_write(&tf->master, req, req_end, tx_tmax_ms, NULL));
        tty_logD(&tf->master);
    }

    {
        char rep[ADU_CAPACITY];
        memset(rep, 0, sizeof(rep));

        const int rx_tmax_ms = 10 + calc_tmin_ms(tf->rtu_config.rate, sizeof(rep));
        const char *const rep_end =
            tty_read(&tf->master, rep, rep + length_of(rep) -1, rx_tmax_ms, NULL);

        EXPECT_NE(rep, rep_end);
        tty_logD(&tf->master);

        EXPECT_EQ(0, check_crc(rep, rep_end));
        EXPECT_EQ(NULL, find_ecode(rep, rep_end));
        EXPECT_EQ(
            (size_t)(rep_end - rep),
            sizeof(modbus_rtu_addr_t) + sizeof(modbus_rtu_fcode_t)
            + sizeof(uint16_t) /* mem addr */ + sizeof(uint8_t) /* count */
            + sizeof(modbus_rtu_crc_t));

        const modbus_rtu_wr_bytes_reply_t *wr_bytes_rep =
            parse_reply_wr_bytes(rep, rep_end);

        EXPECT_NE(NULL, wr_bytes_rep);
        EXPECT_EQ(MAKE_WORD(wr_bytes_rep->mem_addr.low, wr_bytes_rep->mem_addr.high), RTU_ADDR_BASE);
        EXPECT_EQ(wr_bytes_rep->count, length_of(message));
    }
}

UTEST_MAIN();
