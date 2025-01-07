#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "check.h"
#include "log.h"
#include "rtu_cmd.h"
#include "rtu_impl.h"
#include "tty.h"
#include "util.h"

static
void help(const char *argv0, const char *message);

static
const speed_t supported_rates[] =
{
    B1200,
    B2400,
    B4800,
    B9600,
    B19200,
    B57600,
    B115200
};

static
const char *supported_rates_str[] =
{
    "1200",
    "2400",
    "4800",
    "9600",
    "19200",
    "57600",
    "115200"
};

STATIC_ASSERT(
    length_of(supported_rates) == length_of(supported_rates_str),
    "supported rates mismatch");

static
speed_t parse_speed(const char *str)
{
    if(!str) goto fallback;

    for(size_t i = 0; i < length_of(supported_rates_str); ++i)
    {
        if(0 == strcmp(str, supported_rates_str[i])) return supported_rates[i];
    }
fallback:
    logW("unsupported rate %s, fallback to 19200", str ? str : "NULL");
    return B19200;
}

static
parity_t parse_parity(const char *str)
{
    if(!str) goto fallback;
    if(0 == strcmp(str, "E")) return PARITY_even;
    if(0 == strcmp(str, "O")) return PARITY_odd;
    if(0 == strcmp(str, "N")) return PARITY_none;
fallback:
    logW("unsupported parity %s, fallback to Even", str ? str : "NULL");
    return PARITY_even;
}

void help(const char *argv0, const char *message)
{
    if(message) printf("%s: %s\n", argv0, message);
    printf(
        "%s:"
        " -a rtu_address"
        " -d device_path"
        " [-r rate (19200)]"
        " [-p parity E/O/N (E)]"
        " [-t custom 1.5t timeout us]"
        " [-T custom 3.5t timeout us]"
        " [-D tty_debug_size (0)]\n",
        argv0);

    printf("%s: supported rates:\n", argv0);

    const int rates_num = length_of(supported_rates_str);

    for(int i = 0; i < rates_num; ++i)
    {
        printf(
            "%9sbps 1.5t % 6dus 3.5t % 6dus\n",
            supported_rates_str[i],
            calc_1t5_us(supported_rates[i]), calc_3t5_us(supported_rates[i]));
    }

    exit(message ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, const char *argv[])
{
    const char *path = NULL;
    speed_t rate = B19200;
    parity_t parity = PARITY_even;
    int debug_size = 0;
    int addr = -1;
    int timeout_1t5 = -1;
    int timeout_3t5 = -1;

    for(int c; -1 != (c = getopt(argc, (char **)argv, "D:T:a:d:hp:r:t:"));)
    {
        switch(c)
        {
           case 'D':
                debug_size = optarg ? atoi(optarg) : 0;
                break;
           case 'T':
                timeout_3t5 = optarg ? atoi(optarg) : -1;
                break;
           case 'a':
                addr = optarg ? atoi(optarg) : -1;
                break;
            case 'd':
                path = optarg ? strdup(optarg) : NULL;
                break;
            case 'h':
                help(argv[0], NULL);
                break;
            case 'p':
                parity = parse_parity(optarg);
                break;
           case 'r':
                rate = parse_speed(optarg);
                break;
           case 't':
                timeout_1t5 = optarg ? atoi(optarg) : -1;
                break;
           case ':':
            case '?':
            default:
                help(argv[0], "geopt() failure");
                break;
        }
    }

    if(!path) help(argv[0], "device path missing");
    if(-1 == addr) help(argv[0], "address missing");

    rtu_memory_fields_t memory_fields;

    rtu_memory_fields_clear(&memory_fields);
    rtu_memory_fields_init(&memory_fields);

    tty_dev_t dev;

    tty_init(&dev, debug_size);
    tty_open(&dev, path, NULL);
    tty_exclusive_on(dev.fd);
    tty_configure(
        &dev,
        rate, parity, DATA_BITS_8,
        PARITY_none == parity ? STOP_BITS_2 : STOP_BITS_1);

    tty_flush(dev.fd);

    modbus_rtu_run(
        &dev,
        rate,
        addr,
        rtu_pdu_cb,
        timeout_1t5, timeout_3t5,
        (uintptr_t)&memory_fields,
        NULL);

    tty_close(&dev);
    tty_deinit(&dev);
    FREE(path);
    return EXIT_SUCCESS;
}
