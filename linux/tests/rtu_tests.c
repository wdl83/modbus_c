#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <poll.h>
#include <pthread.h>

#include "check.h"
#include "log.h"
#include "master.h"
#include "master_impl.h"
#include "pipe.h"
#include "rtu_impl.h"
#include "tty.h"
#include "tty_pair.h"
#include "utest.h"
#include "util.h"


#define DEBUG_SIZE                                                          1024
#define RTU_ADDR                                                   UINT8_C(0xAA)

typedef modbus_rtu_addr_t addr_t;
typedef modbus_rtu_fcode_t fcode_t;
typedef modbus_rtu_ecode_t ecode_t;
typedef modbus_rtu_mem_addr_t mem_addr_t;
typedef modbus_rtu_count_t count_t;
typedef modbus_rtu_data16_t data16_t;
typedef modbus_rtu_crc_t crc_t;

typedef struct
{
    struct {
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        int rtu_started;
    } sync;
    tty_dev_t *dev;
    speed_t rate;
    modbus_rtu_addr_t self_addr;
    rtu_memory_impl_t memory_impl;
    modbus_rtu_pdu_cb_t pdu_cb;
    int timeout_1t5_us;
    int timeout_3t5_us;
    struct pollfd event;
} rtu_config_t;

struct TestFixture
{
    tty_dev_t master;
    tty_dev_t slave;
    pipe_t channel;
    rtu_config_t rtu_config;
    pthread_t runner;
};

static
char *master_read(struct TestFixture *tf, char *begin, const char *end)
{
    const int tmax_ms = calc_tmax_ms(tf->rtu_config.rate, (size_t)(end - begin));
    char *const curr = tty_read(&tf->master, begin, end, tmax_ms, NULL);
    tty_logD(&tf->master);
    return curr;
}

static
const char *master_write(struct TestFixture *tf, const char *begin, const char *end)
{
    const int tmax_ms = calc_tmax_ms(tf->rtu_config.rate, (size_t)(end - begin));
    const char *const curr = tty_write(&tf->master, begin, end, tmax_ms, NULL);
    tty_logD(&tf->master);
    return curr;
}

static
void *async_run_rtu(void *user_data)
{
    assert(user_data);
    rtu_config_t *config = user_data;
    rtu_memory_impl_t *memory_impl = &config->memory_impl;

    rtu_memory_impl_clear(memory_impl);
    rtu_memory_impl_init(memory_impl);
    memory_impl->self_addr = config->self_addr;

    /* fill with pattern [0, 1 ... 255, 0, 1 .. 255, 0, 1 ..] */
    uint8_t *mem_begin = memory_impl->bytes;
    const uint8_t *const mem_end = mem_begin + sizeof(memory_impl->bytes);

    for(uint8_t pattern = 0; mem_begin != mem_end; ++pattern, ++mem_begin)
        *mem_begin = pattern;

    pthread_mutex_lock(&config->sync.mutex);
    config->sync.rtu_started = 1;
    pthread_mutex_unlock(&config->sync.mutex);
    pthread_cond_signal(&config->sync.cond);

    modbus_rtu_run(
        config->dev,
        config->rate,
        config->timeout_1t5_us, config->timeout_3t5_us,
        config->pdu_cb, (uintptr_t)(&config->memory_impl),
        &config->event);
    return NULL;
}

static
void serial_init(tty_dev_t *master, tty_dev_t *slave)
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
void serial_config(tty_dev_t *master, tty_dev_t *slave, speed_t rate, parity_t parity)
{
    tty_dev_t *devs[] = {master, slave};
    const stop_bits_t stop_bits = PARITY_none == parity ? STOP_BITS_2 : STOP_BITS_1;

    for(tty_dev_t **pdev = devs; pdev != devs + length_of(devs); ++pdev)
        tty_configure(*pdev, rate, parity, DATA_BITS_8, stop_bits);
}

static
void serial_deinit(tty_dev_t *master, tty_dev_t *slave)
{
    tty_deinit(slave);
    tty_deinit(master);
}

UTEST_I_SETUP(TestFixture)
{
    // supports INDEX [0, 7)
    const speed_t rates[] = {B1200, B2400, B4800, B9600, B19200, B57600, B115200};
    const speed_t rate = rates[utest_index];
    struct TestFixture *tf = utest_fixture;
    ASSERT_TRUE(tf);

    logD("%d %dbps", (int)utest_index, tty_bps(rate));

    memset(tf, 0, sizeof(struct TestFixture));

    serial_init(&tf->master, &tf->slave);
    serial_config(&tf->master, &tf->slave, rate, PARITY_none);
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

    pthread_mutex_init(&tf->rtu_config.sync.mutex, NULL);
    pthread_mutex_lock(&tf->rtu_config.sync.mutex);

    CHECK_ERRNO(0 == pthread_create(&tf->runner, NULL, async_run_rtu, &tf->rtu_config));

    // wait for RTU thread to start
    while(!tf->rtu_config.sync.rtu_started)
        pthread_cond_wait(&tf->rtu_config.sync.cond, &tf->rtu_config.sync.mutex);

    pthread_mutex_unlock(&tf->rtu_config.sync.mutex);

    // time required for RTU to transition from INIT to IDLE state (at least 10ms)
    usleep(max(100000, tf->rtu_config.timeout_3t5_us));
}

UTEST_I_TEARDOWN(TestFixture)
{
    struct TestFixture *tf = utest_fixture;
    ASSERT_TRUE(tf);

    const char stop[] = "STOP";

    CHECK_ERRNO(-1 != write(tf->channel.writer, stop, sizeof(stop)));
    CHECK_ERRNO(0 == pthread_join(tf->runner, NULL));
    serial_deinit(&tf->master, &tf->slave);
}

UTEST_I(TestFixture, read_bytes_21, 7)
{
    enum { num = 21 };

    struct TestFixture *tf = utest_fixture;
    ASSERT_TRUE(tf);

    {
        char req[ADU_CAPACITY];
        memset(req, 0, sizeof(req));
        const char *const req_end =
            make_request_rd_bytes(
                tf->rtu_config.self_addr,
                WORD_TO_MEM_ADDR(RTU_MEMORY_ADDR), num,
                req, sizeof(req));
        ASSERT_TRUE(req_end);
        EXPECT_EQ(req_end, master_write(tf,req, req_end));
    }

    {
        char rx_buf[ADU_CAPACITY];
        memset(rx_buf, 0, sizeof(rx_buf));
        const char *const rx_buf_end = master_read(tf, rx_buf, rx_buf + length_of(rx_buf));
        const modbus_rtu_rd_bytes_reply_t *rep = parse_reply_rd_bytes(rx_buf, rx_buf_end - rx_buf);

        ASSERT_TRUE(rep);
        EXPECT_EQ(rep->header.addr, tf->rtu_config.self_addr);
        EXPECT_EQ(rep->header.fcode, FCODE_RD_BYTES);
        EXPECT_EQ(MEM_ADDR_TO_WORD(rep->header.mem_addr), RTU_MEMORY_ADDR);
        EXPECT_EQ(rep->header.count, num);

        for(size_t i = 0; i < rep->header.count; ++i)
            EXPECT_EQ((uint8_t)i, rep->bytes[i]);
    }
}

UTEST_I(TestFixture, write_bytes_func_STR, 7)
{
    struct TestFixture *tf = utest_fixture;
    ASSERT_TRUE(tf);

    const char message[] = "!!!hello this is RTU memory!!!";

    {
        char req[ADU_CAPACITY];
        memset(req, 0, sizeof(req));
        const char *const req_end =
            make_request_wr_bytes(
                tf->rtu_config.self_addr,
                WORD_TO_MEM_ADDR(RTU_MEMORY_ADDR),
                (const uint8_t *)message, length_of(message),
                req, sizeof(req));
        ASSERT_TRUE(req_end);
        EXPECT_EQ(req_end, master_write(tf, req, req_end));
    }

    {
        char rx_buf[ADU_CAPACITY];
        memset(rx_buf, 0, sizeof(rx_buf));
        const char *const rx_buf_end = master_read(tf, rx_buf, rx_buf + sizeof(rx_buf));
        const modbus_rtu_wr_bytes_reply_t *rep = parse_reply_wr_bytes(rx_buf, rx_buf_end - rx_buf);

        ASSERT_TRUE(rep);
        EXPECT_EQ(rep->addr, tf->rtu_config.self_addr);
        EXPECT_EQ(rep->fcode, FCODE_WR_BYTES);
        EXPECT_EQ(MEM_ADDR_TO_WORD(rep->mem_addr), RTU_MEMORY_ADDR);
        EXPECT_EQ(rep->count, length_of(message));
    }
}

UTEST_I(TestFixture, write_bytes_struct_STR, 7)
{
    struct TestFixture *tf = utest_fixture;
    ASSERT_TRUE(tf);

    const char message[] = "!!!hello this is RTU memory!!!";

    {
        struct __attribute__((packed)) {
            modbus_rtu_wr_bytes_request_header_t header;
            char data[sizeof(message)];
            modbus_rtu_crc_t crc;
        } req = {
            .header = {
                .addr = tf->rtu_config.self_addr,
                .fcode = FCODE_WR_BYTES,
                .mem_addr = WORD_TO_MEM_ADDR(RTU_MEMORY_ADDR),
                .count = length_of(message)
            }
        };
        const char *const req_begin = (const char *)&req;
        const char *const req_end = req_begin + sizeof(req);
        memcpy(req.data, message, sizeof(message));
        ASSERT_NE(NULL, implace_crc(&req, sizeof(req)));
        EXPECT_EQ(req_end, master_write(tf, req_begin, req_end));
    }

    {
        modbus_rtu_wr_bytes_reply_t rep;
        char *const rep_begin = (char *)&rep;
        const char *const rep_end = rep_begin + sizeof(rep);
        EXPECT_EQ(rep_end, master_read(tf, rep_begin, rep_end));
        EXPECT_TRUE(valid_crc(&rep, sizeof(rep)));
        EXPECT_EQ(rep.addr, tf->rtu_config.self_addr);
        EXPECT_EQ(rep.fcode, FCODE_WR_BYTES);
        EXPECT_EQ(MEM_ADDR_TO_WORD(rep.mem_addr), RTU_MEMORY_ADDR);
        EXPECT_EQ(rep.count, length_of(message));
    }
}

/* check if both methods produce same requests */
UTEST(rtu_tests, wr_bytes_request)
{
    const char msg[] = "!!!hello this is RTU memory!!!";

    // method A

    struct __attribute__((packed)) {
        modbus_rtu_wr_bytes_request_header_t header;
        char data[sizeof(msg)];
        modbus_rtu_crc_t crc;
    } reqA = {
        .header = {
            .addr = 0xAB,
            .fcode = FCODE_WR_BYTES,
            .mem_addr = WORD_TO_MEM_ADDR(0x1234),
            .count = length_of(msg)
        }
    };

    memcpy(reqA.data, msg, sizeof(msg));
    ASSERT_NE(NULL, implace_crc(&reqA, sizeof(reqA)));

    // method B

    char reqB[ADU_CAPACITY];

    memset(reqB, 0, sizeof(reqB));

    const char *const reqB_end =
        make_request_wr_bytes(
            0xAB,
            WORD_TO_MEM_ADDR(0x1234),
            (const uint8_t *)msg, length_of(msg),
            reqB, sizeof(reqB));
    ASSERT_NE(NULL, reqB_end);

    // must produce same results

    EXPECT_EQ((size_t)(reqB_end - reqB), sizeof(reqA));
    EXPECT_EQ(0, memcmp(&reqA, reqB, sizeof(reqA)));
}

UTEST_I(TestFixture, read_holding_registers_33, 7)
{
    enum { num = 33 };

    struct TestFixture *tf = utest_fixture;
    ASSERT_TRUE(tf);

    {
        modbus_rtu_rd_holding_registers_request_t req = {
            .addr = tf->rtu_config.self_addr,
            .fcode = FCODE_RD_HOLDING_REGISTERS,
            .mem_addr = WORD_TO_MEM_ADDR(RTU_MEMORY_ADDR),
            .count = WORD_TO_COUNT(num)
        };
        const char *const req_begin = (const char *)&req;
        const char *const req_end = req_begin + sizeof(req);

        ASSERT_NE(NULL, implace_crc(&req, sizeof(req)));
        EXPECT_EQ(req_end, master_write(tf, req_begin, req_end));
    }
    {
        struct __attribute__((packed)) {
            modbus_rtu_rd_holding_registers_reply_header_t header;
            modbus_rtu_data16_t data[num];
            modbus_rtu_crc_t crc;
        } rep;
        char *const rep_begin = (char *)&rep;
        const char *const rep_end = rep_begin + sizeof(rep);

        memset(&rep, 0, sizeof(rep));
        const char *const curr = master_read(tf, rep_begin, rep_end);
        EXPECT_EQ(curr, rep_end);
        EXPECT_EQ(rep.header.addr, tf->rtu_config.self_addr);
        EXPECT_EQ(rep.header.fcode, FCODE_RD_HOLDING_REGISTERS);
        EXPECT_EQ(rep.header.byte_count, num << 1); // byte count

        /* rtu memory fill pattern [0, 1 ... 255, 0, 1 .. 255, ..] (every byte)
         * however rtu_memory implementation is done for 8-bit uC
         * so every byte of addressable memory is treated as register
         * with most significant byte set to 0x00 */
        for(size_t i = 0; i < length_of(rep.data); ++i)
        {
            EXPECT_EQ(UINT8_C(0), rep.data[i].high);
            EXPECT_EQ((uint8_t)i, rep.data[i].low);
        }

        EXPECT_TRUE(valid_crc(&rep, sizeof(rep)));
    }
}

UTEST_I(TestFixture, read_wr_register_0x00AB_offset_32, 7)
{
    struct TestFixture *tf = utest_fixture;
    ASSERT_TRUE(tf);

    {
        modbus_rtu_wr_register_request_t req = {
            .addr = tf->rtu_config.self_addr,
            .fcode = FCODE_WR_REGISTER,
            .mem_addr = WORD_TO_MEM_ADDR(RTU_MEMORY_ADDR + 32),
            .data = {.high = 0, .low = 0xAB}
        };
        const char *const req_begin = (const char *)&req;
        const char *const req_end = req_begin + sizeof(req);

        ASSERT_NE(NULL, implace_crc(&req, sizeof(req)));
        EXPECT_EQ(req_end, master_write(tf, req_begin, req_end));
    }
    {
        modbus_rtu_wr_register_reply_t rep;
        char *const rep_begin = (char *)&rep;
        const char *const rep_end = rep_begin + sizeof(rep);

        memset(&rep, 0, sizeof(rep));
        const char *const curr = master_read(tf, rep_begin, rep_end);
        EXPECT_EQ(curr, rep_end);
        EXPECT_EQ(rep.addr, tf->rtu_config.self_addr);
        EXPECT_EQ(rep.fcode, FCODE_WR_REGISTER);
        EXPECT_EQ(MEM_ADDR_TO_WORD(rep.mem_addr), RTU_MEMORY_ADDR + 32);
        EXPECT_EQ(UINT8_C(0), rep.data.high);
        EXPECT_EQ(UINT8_C(0xAB), rep.data.low);
        EXPECT_TRUE(valid_crc(&rep, sizeof(rep)));
    }
}

UTEST_I(TestFixture, master_write_read_bytes, 7)
{
    struct TestFixture *tf = utest_fixture;
    ASSERT_TRUE(tf);

    const uint8_t tx_buf[] = "read/write bytes with master_impl";

    rtu_master_impl_t impl = {.dev = &tf->master, .rate = tf->rtu_config.rate};

    const uint8_t *const tx_end =
        rtu_master_wr_bytes(
            &impl,
            tf->rtu_config.self_addr,
            WORD_TO_MEM_ADDR(RTU_MEMORY_ADDR),
            length_of(tx_buf), tx_buf);

    EXPECT_EQ(tx_end, tx_buf + sizeof(tx_buf));

    usleep(100000); // 10ms, for RTU to transition from BUSY to IDLE state

    uint8_t rx_buf[sizeof(tx_buf)];

    memset(rx_buf, 0, sizeof(rx_buf));

    uint8_t *const rx_end =
        rtu_master_rd_bytes(
            &impl,
            tf->rtu_config.self_addr,
            WORD_TO_MEM_ADDR(RTU_MEMORY_ADDR),
            length_of(rx_buf), rx_buf);

    EXPECT_EQ(rx_end, rx_buf + sizeof(rx_buf));
    EXPECT_EQ(0, memcmp(rx_buf, tx_buf, sizeof(rx_buf)));
}

UTEST_I(TestFixture, master_write_read_holding_registers, 7)
{
    struct TestFixture *tf = utest_fixture;
    ASSERT_TRUE(tf);

    const data16_t tx_data[] =
    {
        WORD_TO_DATA16(0x0012),
        WORD_TO_DATA16(0x0034),
        WORD_TO_DATA16(0x0056),
        WORD_TO_DATA16(0x0078),
        WORD_TO_DATA16(0x009A),
        WORD_TO_DATA16(0x00BC),
        WORD_TO_DATA16(0x00DE),
        WORD_TO_DATA16(0x00F0)
    };
    rtu_master_impl_t impl = {.dev = &tf->master, .rate = tf->rtu_config.rate};

    const data16_t *const tx_data_end =
        rtu_master_wr_registers(
            &impl,
            tf->rtu_config.self_addr,
            WORD_TO_MEM_ADDR(RTU_MEMORY_ADDR),
            WORD_TO_COUNT(length_of(tx_data)), tx_data);

    EXPECT_EQ(tx_data_end, tx_data + length_of(tx_data));

    usleep(100000); // 10ms, for RTU to transition from BUSY to IDLE state

    data16_t rx_data[length_of(tx_data)];

    memset(rx_data, 0, sizeof(rx_data));

    data16_t *const rx_data_end =
        rtu_master_rd_holding_registers(
            &impl,
            tf->rtu_config.self_addr,
            WORD_TO_MEM_ADDR(RTU_MEMORY_ADDR),
            WORD_TO_COUNT(length_of(rx_data)), rx_data);

    EXPECT_EQ(rx_data_end, rx_data + length_of(rx_data));
    EXPECT_EQ(0, memcmp(tx_data, rx_data, sizeof(rx_data)));
}


UTEST_MAIN();
