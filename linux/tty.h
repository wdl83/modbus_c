#pragma once

#include <stddef.h>
#include <stdint.h>

#include <termios.h>

struct pollfd;

/* speed_t
 *
 * B1200,
 * B2400,
 * B4800,
 * B9600,
 * B19200,
 * B38400,
 * B57600,
 * B115200 */

typedef enum
{
    PARITY_none = 'N',
    PARITY_odd  = 'O',
    PARITY_even = 'E'
} parity_t;

typedef enum
{
    DATA_BITS_5 = 5,
    DATA_BITS_6 = 6,
    DATA_BITS_7 = 7,
    DATA_BITS_8 = 8
} data_bits_t;

typedef enum
{
    STOP_BITS_1 = 1,
    STOP_BITS_2 = 2
} stop_bits_t;

typedef struct tty_dev
{
    int fd;
    char *path;
    struct termios config;
    struct
    {
        char *begin;
        char *curr;
        char *end;
    } debug;
} tty_dev_t;

void tty_init(tty_dev_t *, size_t debug_size);
void tty_deinit(tty_dev_t *);
// if NULL == flags, default flags will be used: O_RDWR | O_NONBLOCK
void tty_open(tty_dev_t *, const char *path, int *user_flags);
void tty_adopt(tty_dev_t *, int fd);
void tty_close(tty_dev_t *);
void tty_configure(tty_dev_t *, speed_t, parity_t, data_bits_t, stop_bits_t);
char *tty_read(
    tty_dev_t *,
    char *begin, const char *end,
    int timeout,
    struct pollfd *aux);
// low latency
char *tty_read_ll(tty_dev_t *, char *begin, const char *end, int delay_us);
const char *tty_write(
    tty_dev_t *,
    const char *begin, const char *end,
    int timeout,
    struct pollfd *aux);
/* discards data, received but not read (rx), written but not transmitted (tx) */
void tty_flush_rx(int fd);
void tty_flush_tx(int fd);
void tty_flush(int fd);
// wait until all data written is transmitted
void tty_drain(int fd);
/* request exclusive mode, open on same fd will result in EBUSY */
void tty_exclusive_on(int fd);
void tty_exclusive_off(int fd);
void tty_get_term_config(int fd, struct termios *);
void tty_set_term_config(int fd, const struct termios *);
void tty_configure_term(struct termios *, speed_t, parity_t, data_bits_t, stop_bits_t);
int tty_bps(speed_t);
const char *tty_rate_str(speed_t);
void tty_logD(tty_dev_t *);
