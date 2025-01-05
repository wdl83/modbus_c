#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <linux/serial.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "check.h"
#include "log.h"
#include "time_util.h"
#include "tty.h"
#include "util.h"

static
char *dump(char *dst, const char *const dst_end, const char *begin, const char *const end)
{
    CHECK(dst);
    CHECK(dst_end);
    CHECK(begin);
    CHECK(end);

    int num = 0;

    while(begin != end)
    {
        const int value = (unsigned char)*begin;
        ++begin;

        if(0 == value)
        {
            ++num;
            if(begin != end) continue;
        }

        if(0 < num)
        {
            if(1 < num)
            {
                const int len = snprintf(dst, dst_end - dst, "%dx", num);
                dst += min(len, dst_end - dst);
            }
            {
                const int len = snprintf(dst, dst_end - dst, "%02x", 0);
                dst += min(len, dst_end - dst);
            }
            if(begin != end)
            {
                const int len = snprintf(dst, dst_end - dst, " ");
                dst += min(len, dst_end - dst);
            }
            num = 0;
        }

        if(0 != value)
        {
            const int len = snprintf(dst, dst_end - dst, "%02x", value);
            dst += min(len, dst_end - dst);
        }

        if(begin != end)
        {
            const int len = snprintf(dst, dst_end - dst, " ");
            dst += min(len, dst_end - dst);
        }
    }
    return dst;
}

static
void debug(
    tty_dev_t *dev,
    const char *tag, int64_t timeout_us, int64_t duration_us,
    const char *begin, const char *const end, const char *const curr)
{
    CHECK(dev);
    CHECK(tag);
    CHECK(begin);
    CHECK(end);
    CHECK(curr);

    char *dst = dev->debug.curr;
    const char *const dst_end = dev->debug.end;

    if(!dst || !dst_end) return;

    int len = snprintf(
        dst, dst_end - dst,
        "%s %" PRId64 "us/%" PRId64 "us (%zd) ",
        tag, duration_us, timeout_us, curr - begin);

    dst += min(len, dst_end - dst);
    dst = dump(dst, dst_end, begin, end);
    len = snprintf(dst, dst_end - dst, "%s\n", curr == begin ? " timeout" : "");
    dst += min(len, dst_end - dst);
    dev->debug.curr = dst;
}

void tty_init(tty_dev_t *dev, size_t debug_size)
{
    CHECK(dev);
    memset(dev, 0, sizeof(tty_dev_t));
    dev->fd = -1;
    if(debug_size)
    {
        CHECK_ERRNO(NULL != (dev->debug.begin = malloc(debug_size)));
        dev->debug.curr = dev->debug.begin;
        dev->debug.end = dev->debug.begin + debug_size;
        memset(dev->debug.begin, 0, debug_size);
    }
    logT("%p debug_size %zu", dev, debug_size);
}

void tty_deinit(tty_dev_t *dev)
{
    if(!dev) return;

    tty_close(dev);

    if(dev->path)
    {
        free(dev->path);
        dev->path = NULL;
    }

    if(dev->debug.begin)
    {
        free(dev->debug.begin);
        dev->debug.begin = NULL;
        dev->debug.curr = NULL;
        dev->debug.end = NULL;
    }
    logT("%p", dev);
}

void tty_open(tty_dev_t *dev, const char *path, int *user_flags)
{
    const int flags = user_flags ? *user_flags : O_RDWR | O_NONBLOCK;
    CHECK(dev);
    CHECK(-1 == dev->fd);
    CHECK(path);
    CHECK(!dev->path);
    CHECK_ERRNO(NULL != (dev->path = strdup(path)));
    CHECK_ERRNO(-1 != (dev->fd = open(dev->path, flags)));
    logT("%p %s (%d)", dev, path, dev->fd);
}

void tty_adopt(tty_dev_t *dev, int fd)
{
    CHECK(dev);
    CHECK(-1 == dev->fd);
    CHECK(-1 != fd);
    dev->fd = fd;
    logT("%p (%d)", dev, dev->fd);
}

void tty_close(tty_dev_t *dev)
{
    if(!dev) return;

    if(dev->path)
    {
        free(dev->path);
        dev->path = NULL;
    }

    if(-1 != dev->fd) CHECK_ERRNO(0 == close(dev->fd));
    logT("%p %d", dev, dev->fd);
    dev->fd = -1;
}

void tty_configure(
    tty_dev_t *dev,
    speed_t speed, parity_t parity, data_bits_t data_bits, stop_bits_t stop_bits)
{
    CHECK(dev);
    memset(&dev->config, 0, sizeof(dev->config));
    tty_configure_term(&dev->config, speed, parity, data_bits, stop_bits);
    tty_set_term_config(dev->fd, &dev->config);
    logT("%d", dev->fd);
}

void validate_syscall_result(int r)
{
    CHECK_ERRNO(-1 != r || -1 == r && EINTR == errno);
    if(-1 == r) logT("%s", strerror(errno));
}

char *tty_read(
    tty_dev_t *dev,
    char *const begin, const char *const end,
    const int timeout,
    const int event_fd)
{
    CHECK(dev);
    CHECK(begin);
    CHECK(end);
    CHECK(-1 != dev->fd);

    struct pollfd events[] =
    {
        {dev->fd, (short)POLLIN /* events */, (short)0 /* revents */},
        {event_fd, (short)POLLIN /* events */, (short)0 /* revents */}
    };

    const int64_t start_ts = timestamp_us();
    int64_t elapsed = 0;
    int elapsed_ms = 0;
    char *curr = begin;

    while(curr != end && (0 > timeout || timeout >= elapsed_ms))
    {
        int r = poll(events, length_of(events), timeout - elapsed_ms);

        validate_syscall_result(r);
        elapsed = timestamp_us() - start_ts;
        elapsed_ms = elapsed / 1000;

        if(0 == r) continue; // poll timeout

        if(events[0].revents & POLLIN)
        {
            r = read(dev->fd, curr, end - curr);
            validate_syscall_result(r);
            CHECK(0 != r);
            curr += r;
            if(0 > timeout) break;
        }

        if(events[1].revents & POLLIN) break;
    }

    debug(dev, __FUNCTION__, (int64_t)timeout * 1000, timestamp_us() - start_ts, begin, end, curr);
    return curr;
}

char *tty_read_ll(tty_dev_t *dev, char *const begin, const char *const end, const int delay_us)
{
    CHECK(dev);
    CHECK(begin);
    CHECK(end);
    CHECK(-1 != dev->fd);
    const int64_t start_us = timestamp_us();
    int64_t elapsed_us = 0;
    char *curr = begin;

    for(;delay_us > elapsed_us;)
    {
        if(0 != elapsed_us)
        {
            if(-1 == usleep(delay_us - elapsed_us)) CHECK_ERRNO(errno == EINTR);
        }

        const int r = read(dev->fd, curr, end - curr);

        if(0 < r)
        {
            curr += r;
            break;
        }

        // EAGAIN should not be reported, see VTIME/VMIN
        if(0 != r) CHECK_ERRNO(errno == EINTR);
        elapsed_us += timestamp_us() - start_us;
    }
    debug(dev, __FUNCTION__, delay_us, timestamp_us() - start_us, begin, end, curr);
    return curr;
}

const char *tty_write(
    tty_dev_t *dev,
    const char *begin, const char *const end,
    const int timeout,
    const int event_fd)
{
    CHECK(dev);
    CHECK(begin);
    CHECK(end);
    CHECK(-1 != dev->fd);

    struct pollfd events[] =
    {
        {dev->fd, (short)POLLOUT /* events */, (short)0 /* revents */},
        {event_fd, (short)POLLIN /* events */, (short)0 /* revents */}
    };

    const int64_t start_ts = timestamp_us();
    int64_t elapsed = 0;
    int elapsed_ms = 0;
    const char *curr = begin;

    while(curr != end && (0 > timeout || timeout >= elapsed_ms))
    {
        int r = poll(events, length_of(events), timeout - elapsed_ms);

        validate_syscall_result(r);
        elapsed = timestamp_us() - start_ts;
        elapsed_ms = elapsed / 1000;

        if(0 == r) continue; // poll timeout

        if(events[0].revents & POLLOUT)
        {
            r = write(dev->fd, curr, end - curr);
            validate_syscall_result(r);
            CHECK(0 != r);
            curr += r;
        }

        if(events[1].revents & POLLIN) break;
    }

    debug(dev, __FUNCTION__, (int64_t)timeout * 1000, timestamp_us() - start_ts, begin, end, curr);
    return curr;
}

void tty_flush_rx(int fd)
{
    CHECK(-1 != fd);
    CHECK_ERRNO(-1 != tcflush(fd, TCIFLUSH));
    logT("%d", fd);
}

void tty_flush_tx(int fd)
{
    CHECK(-1 != fd);
    CHECK_ERRNO(-1 != tcflush(fd, TCOFLUSH));
    logT("%d", fd);
}

void tty_flush(int fd)
{
    CHECK(-1 != fd);
    CHECK_ERRNO(-1 != tcflush(fd, TCIOFLUSH));
    logT("%d", fd);
}

void tty_drain(int fd)
{
    CHECK(-1 != fd);
    CHECK_ERRNO(-1 != tcdrain(fd));
    logT("%d", fd);
}


void tty_exclusive_on(int fd)
{
    CHECK(-1 != fd);
    CHECK_ERRNO(0 == ioctl(fd, TIOCEXCL));
    logT("%d", fd);
}

void tty_exclusive_off(int fd)
{
    CHECK(-1 != fd);
    CHECK_ERRNO(0 == ioctl(fd, TIOCNXCL));
    logT("%d", fd);
}

void tty_get_term_config(int fd, struct termios *config)
{
    CHECK(-1 != fd);
    CHECK(config);
    CHECK_ERRNO(-1 != tcgetattr(fd, config));
    logT("%d", fd);
}

void tty_set_term_config(int fd, const struct termios *config)
{
    CHECK(-1 != fd);
    CHECK(config);
    CHECK_ERRNO(-1 != tcsetattr(fd, TCSANOW, config));

    struct termios current;

    memset(&current, 0, sizeof(current));
    tty_get_term_config(fd, &current);
    CHECK(0 == memcmp(config, &current, sizeof(struct termios)));
#if TTY_ASYNC_LOW_LATENCY
    {
        struct serial_struct serial;

        ioctl(fd, TIOCGSERIAL, &serial);
        serial.flags |= ASYNC_LOW_LATENCY;
        ioctl(fd, TIOCSSERIAL, &serial);
    }
#endif
    logT("%d", fd);
}

void tty_configure_term(
    struct termios *config,
    speed_t speed, parity_t parity, data_bits_t data_bits, stop_bits_t stop_bits)
{
    CHECK(config);

    // rate
    {
        cfsetispeed(config, speed);
        cfsetospeed(config, speed);
    }
    // parity
    {
        if(PARITY_none == parity)
        {
            config->c_cflag &= ~PARENB;
            config->c_iflag &= ~INPCK;
        }
        else
        {
            config->c_iflag |= INPCK;
            //config->c_iflag &= ~IGNPAR;
            config->c_cflag |= PARENB;

            if(PARITY_odd == parity) config->c_cflag |= PARODD;
            else if(PARITY_even == parity) config->c_cflag &= ~PARODD;
        }
    }
    // data bits
    {
        config->c_cflag &= ~CSIZE;
        if(DATA_BITS_5 == data_bits) config->c_cflag |= CS5;
        else if(DATA_BITS_6 == data_bits) config->c_cflag |= CS6;
        else if(DATA_BITS_7 == data_bits) config->c_cflag |= CS7;
        else if(DATA_BITS_8 == data_bits) config->c_cflag |= CS8;
    }
    // stop bits
    {
        if(STOP_BITS_1 == stop_bits) config->c_cflag &= ~CSTOPB;
        else if(STOP_BITS_2 == stop_bits) config->c_cflag |= CSTOPB;
    }
    // aux (similar to cfmakeraw())
    {
        config->c_cflag &= ~CRTSCTS;
        // enable receiver
        config->c_cflag |= CREAD;
        // ignore modem control lines
        config->c_cflag |= CLOCAL;
        // disable canonical mode (line-by-line processing)
        config->c_lflag &= ~ICANON;
        // disable input char echo
        config->c_lflag &= ~ECHO;
        // disable special character interpretation
        config->c_lflag &= ~ISIG;
        // disable SW flow control
        config->c_iflag &= ~(IXON | IXOFF |IXANY);
        // disable spacial character processing
        config->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
        // disable impl. defined output processing
        config->c_oflag &= ~OPOST;
        // do not convert '\n' to '\r\n'
        config->c_oflag &= ~ONLCR;
        /* dont block read syscall (poll syscall will be used to monitor fd)
         * source: man termios
         * If data is available, read(2) returns immediately, with
         * the lesser of the number of bytes available, or the number
         * of bytes requested.  If no data is available, read(2) returns 0. */
        config->c_cc[VTIME] = 0;
        config->c_cc[VMIN] = 0;
    }
}

int tty_bps(speed_t rate)
{
    switch(rate)
    {
        case B1200: return 1200;
        case B2400: return 2400;
        case B4800: return 4800;
        case B9600: return 9600;
        case B19200: return 19200;
        case B38400: return 38400;
        case B57600: return 57600;
        case B115200: return 115200;
        default:
            CHECK(0 && "unsupported rate");
            return -1;
    }
}
