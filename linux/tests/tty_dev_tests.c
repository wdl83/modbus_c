#include <ctype.h>
#include <stdatomic.h>
#include <stddef.h>
#include <string.h>

#include <pthread.h>
#include <termios.h>

#include "utest.h"

#include "buf.h"
#include "log.h"
#include "tty.h"
#include "tty_pair.h"
#include "util.h"


#ifdef NDEBUG
#define DEBUG_SIZE 0
#else
#define DEBUG_SIZE 1024
#endif

static
void init(tty_dev_t *master, tty_dev_t *slave)
{
    tty_pair_t pair;

    tty_pair_init(&pair);
    tty_pair_create(&pair, TTY_DEFAULT_MULTIPLEXOR, NULL);
    tty_init(master, DEBUG_SIZE);
    tty_init(slave, DEBUG_SIZE);
    tty_adopt(master, pair.master_fd);
    tty_open(slave, pair.slave_path, NULL);
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

    const char *curr = tty_write(&master, begin, end, timeout, NULL);
    EXPECT_TRUE(curr == end);

    char buf[255];

    curr = tty_read(&slave, buf, buf + sizeof(buf), timeout, NULL);
    EXPECT_TRUE((size_t)(curr - buf) == size);
    EXPECT_TRUE(0 == memcmp(message, buf, size));

    deinit(&master, &slave);
}

typedef struct async_data
{
    tty_dev_t *dev;
    int timeout;

} async_data_t;

static
void *async_read(void *user_data)
{
    CHECK(user_data);

    async_data_t *data = (async_data_t *)user_data;
    tty_dev_t *dev = data->dev;
    const size_t buf_size = 255;
    buf_t *buf = buf_alloc(buf_size);
    buf->curr = tty_read(dev, buf->begin, buf->end, data->timeout, NULL);
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
    async_data_t data =
    {
        .dev = &slave,
        .timeout = timeout
    };
    pthread_t receiver;

    CHECK_ERRNO(0 == pthread_create(&receiver, NULL, async_read, &data));
    const char *curr = tty_write(&master, msg, msg + msg_size, timeout, NULL);
    EXPECT_TRUE(curr == msg + msg_size);

    buf_t *recv_buf = NULL;

    CHECK_ERRNO(0 == pthread_join(receiver, (void **)&recv_buf));

    EXPECT_TRUE((size_t)(recv_buf->curr - recv_buf->begin) == msg_size);
    EXPECT_TRUE(0 == memcmp(recv_buf->begin, msg, msg_size));

    buf_free(&recv_buf);
    deinit(&master, &slave);
}

static
void *async_echo(void *user_data)
{
    CHECK(user_data);

    const char stop[] = "STOP";
    async_data_t *data = (async_data_t *)user_data;
    tty_dev_t *dev = data->dev;
    char buf[255];

    memset(buf, 0, length_of(buf));

    for(;;)
    {
        const int rx_timeout = rand() % data->timeout;
        const int tx_timeout = rand() % data->timeout;
        const char *next = buf;

        for(char *curr = buf; curr == buf; next = curr)
            curr = tty_read(dev, curr, buf + length_of(buf), rx_timeout, NULL);


        for(const char *curr = buf; curr != next;)
            curr = tty_write(dev, curr, next, tx_timeout, NULL);

        if(0 == memcmp(stop, buf, length_of(stop))) return NULL;
    }
}

UTEST(tty_dev, async_echo)
{
    tty_dev_t master, slave;

    init(&master, &slave);
    config(&master, &slave, B57600, PARITY_none);

    const char message[] = "*** hello on other side! ?\t? \n12 _ $ test \n message *** STOP";
    const char *message_end = message + length_of(message);
    const int timeout = 10;
    const int tx_timeout = rand() % timeout;
    const int rx_timeout = rand() % timeout;
    async_data_t data =
    {
        .dev = &slave,
        .timeout = 20
    };

    pthread_t receiver;
    CHECK_ERRNO(0 == pthread_create(&receiver, NULL, async_echo, &data));

    // send 1 word at a time
    for(const char *w_begin = message, *w_end = message; w_begin != message_end; w_begin = w_end)
    {
        while(w_begin != message_end && isspace(*w_begin)) ++w_begin;
        w_end = w_begin;
        while(w_end != message_end && !isspace(*w_end)) ++w_end;

        const size_t len = w_end - w_begin;

        if(!len) break;

        for(const char *curr = w_begin; curr != w_end;)
            curr = tty_write(&master, curr, w_end, timeout, NULL);

        // receive echo and check if it matches
        char rx_buf[255] = {};
        const char *next = rx_buf;

        ASSERT_TRUE(length_of(rx_buf) >= len);

        for(char *curr = rx_buf; len != (size_t)(curr - rx_buf); next = curr)
            curr = tty_read(&master, curr, rx_buf + length_of(rx_buf), timeout, NULL);

        EXPECT_TRUE((size_t)(next - rx_buf) == len);
        EXPECT_TRUE(0 == memcmp(w_begin, rx_buf, len));
    }

    CHECK_ERRNO(0 == pthread_join(receiver, NULL));

    deinit(&master, &slave);
}


atomic_int usr1_cntr = 0;

static
void handle_USR1(int sig_no)
{
    if(SIGUSR1 == sig_no) ++usr1_cntr;
}

UTEST(dev_tty_t, read_interrupted)
{
    struct sigaction action_backup;

    set_signal_handler(SIGUSR1, handle_USR1, &action_backup);

    tty_dev_t master, slave;

    init(&master, &slave);
    config(&master, &slave, B2400, PARITY_none);

    async_data_t data =
    {
        .dev = &slave,
        .timeout = 1000
    };
    pthread_t receiver;
    buf_t *recv_buf = NULL;

    CHECK_ERRNO(0 == pthread_create(&receiver, NULL, async_read, &data));
    usleep(10000);
    pthread_kill(receiver, SIGUSR1);
    CHECK_ERRNO(0 == pthread_join(receiver, (void **)&recv_buf));
    buf_free(&recv_buf);
    deinit(&master, &slave);
    EXPECT_TRUE(1 == usr1_cntr);

    restore_signal_handler(SIGUSR1, &action_backup);
}

UTEST_MAIN();
