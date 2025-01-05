#include <pthread.h>
#include <termios.h>

#include "utest.h"

#include "log.h"
#include "tty.h"
#include "tty_pair.h"
#include "util.h"

typedef struct buf
{
    char *begin;
    char *curr;
    char *end;
} buf_t;

static
buf_t *buf_alloc(size_t size)
{
    void *p = malloc(sizeof(buf_t) + size);
    CHECK_ERRNO(p);
    memset(p, 0, sizeof(buf_t) + size);
    buf_t *buf = (buf_t *)p;
    buf->begin = p + sizeof(buf_t);
    buf->curr = buf->begin;
    buf->end = buf->begin + size;
    return buf;
}

static
void buf_free(buf_t **buf)
{
    if(!buf || !*buf) return;
    free(*buf);
    *buf = NULL;
}

static
void init(tty_dev_t *master, tty_dev_t *slave)
{
    tty_pair_t pair;

    tty_pair_init(&pair);
    tty_pair_create(&pair, TTY_DEFAULT_MULTIPLEXOR, NULL);
    tty_init(master, 0);
    tty_init(slave, 0);
    tty_adopt(master, pair.master_fd);
    tty_open(slave, pair.slave_path, NULL);
    tty_pair_deinit(&pair);
}

static
void config(tty_dev_t *master, tty_dev_t *slave, speed_t rate, parity_t parity)
{
    tty_configure(
        master,
        rate,
        parity,
        DATA_BITS_8,
        PARITY_none == parity ? STOP_BITS_2 : STOP_BITS_1);
    tty_configure(
        slave,
        rate,
        parity,
        DATA_BITS_8, PARITY_none == parity ? STOP_BITS_2 : STOP_BITS_1);
}

static
void deinit(tty_dev_t *master, tty_dev_t *slave)
{
    tty_deinit(slave);
    tty_deinit(master);
}

UTEST(tty_dev, open_and_close)
{
    tty_dev_t master, slave;

    init(&master, &slave);
    EXPECT_TRUE(-1 != master.fd);
    EXPECT_TRUE(-1 != slave.fd);
    deinit(&master, &slave);
}

UTEST(tty_dev, write_then_read)
{
    tty_dev_t master, slave;

    init(&master, &slave);
    config(&master, &slave, B57600, PARITY_none);

    const char message[] = "hello on other side!";
    const int timeout = 100;
    const char *const begin = message;
    const char *const end = begin + length_of(message);
    const size_t size = end - begin;

    const char *curr = tty_write(&master, begin, end, timeout, -1);
    EXPECT_TRUE(curr == end);

    char buf[255];

    curr = tty_read(&slave, buf, buf + sizeof(buf), timeout, -1);
    EXPECT_TRUE((size_t)(curr - buf) == size);
    EXPECT_TRUE(0 == memcmp(message, buf, size));

    deinit(&master, &slave);
}

typedef struct test_data
{
    tty_dev_t *dev;
    const char *begin;
    const char *end;
    int timeout;

} test_data_t;

static
void *async_read(void *user_data)
{
    CHECK(user_data);

    test_data_t *data = (test_data_t *)user_data;
    tty_dev_t *dev = data->dev;
    const size_t size = data->end - data->begin;
    const size_t buf_size = 255;
    buf_t *buf = buf_alloc(buf_size);
    buf->curr = tty_read(dev, buf->begin, buf->end, data->timeout, -1);
    return buf;
}

UTEST(tty_dev, async_read_and_write)
{
    tty_dev_t master, slave;

    init(&master, &slave);
    config(&master, &slave, B57600, PARITY_none);

    const char msg[] = "hello on other side!";
    const size_t msg_size = length_of(msg);
    const int timeout = 100;
    test_data_t data =
    {
        .dev = &slave,
        .begin = msg,
        .end = msg + msg_size,
        .timeout = timeout
    };
    pthread_t receiver;

    CHECK_ERRNO(0 == pthread_create(&receiver, NULL, async_read, &data));
    const char *curr = tty_write(&master, msg, msg + msg_size, timeout, -1);
    EXPECT_TRUE(curr == msg + msg_size);

    buf_t *recv_buf = NULL;

    CHECK_ERRNO(0 == pthread_join(receiver, (void **)&recv_buf));

    EXPECT_TRUE((size_t)(recv_buf->curr - recv_buf->begin) == msg_size);
    EXPECT_TRUE(0 == memcmp(recv_buf->begin, msg, msg_size));

    buf_free(&recv_buf);
    deinit(&master, &slave);
}

UTEST_MAIN();
