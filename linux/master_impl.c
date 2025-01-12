#include "check.h"
#include "master_impl.h"
#include "rtu_impl.h"
#include "tty.h"


typedef modbus_rtu_addr_t addr_t;
typedef modbus_rtu_fcode_t fcode_t;
typedef modbus_rtu_mem_addr_t mem_addr_t;
typedef modbus_rtu_count_t count_t;
typedef modbus_rtu_data16_t data16_t;
typedef modbus_rtu_crc_t crc_t;

typedef modbus_rtu_wr_bytes_request_header_t wr_bytes_request_header_t;
typedef modbus_rtu_wr_bytes_reply_t wr_bytes_reply_t;

typedef modbus_rtu_rd_bytes_request_t rd_bytes_request_t;
typedef modbus_rtu_rd_bytes_reply_header_t rd_bytes_reply_header_t;
typedef modbus_rtu_rd_bytes_reply_t rd_bytes_reply_t;


static
const void *write_impl(rtu_master_impl_t *impl, const void *const data, const size_t size)
{
    const char *const begin = data;
    const char *const end = begin + size;
    const int tmax_ms = calc_tmax_ms(impl->rate, size);
    const char *const curr = tty_write(impl->dev, begin, end, tmax_ms, NULL);
    tty_logD(impl->dev);
    return end == curr ? curr : NULL;
}

static
void *read_impl(rtu_master_impl_t *impl, void *const data, const size_t size)
{
    char *const begin = data;
    char *const end = begin + size;
    const int tmax_ms = calc_tmax_ms(impl->rate, size);
    char *const curr = tty_read(impl->dev, begin, end, tmax_ms, NULL);
    tty_logD(impl->dev);
    return end == curr ? curr : NULL;
}

const void *rtu_master_write_bytes(
    rtu_master_impl_t *const impl,
    const addr_t addr,
    const mem_addr_t mem_addr, const uint8_t count, const uint8_t *const bytes)
{
    char tx_buf[ADU_CAPACITY];

    const char *req_end =
        make_request_wr_bytes(addr, mem_addr, bytes, count, tx_buf, sizeof(tx_buf));

    if(!req_end) return NULL;

    if(!write_impl(impl, tx_buf, (size_t)(req_end - tx_buf))) return NULL;

    wr_bytes_reply_t reply;

    if(!read_impl(impl, &reply, sizeof(reply))) return NULL;

    if(!parse_reply_wr_bytes(&reply, sizeof(reply))) return NULL;
    return bytes + count;
}

void *rtu_master_read_bytes(
    rtu_master_impl_t *const impl,
    const addr_t addr,
    const mem_addr_t mem_addr, const uint8_t count, uint8_t *const bytes)
{
    rd_bytes_request_t req =
    {
        .addr = addr,
        .fcode = FCODE_RD_BYTES,
        .mem_addr = mem_addr,
        .count = count
    };

    if(!implace_crc(&req, sizeof(req))) return NULL;
    if(!write_impl(impl, &req, sizeof(req))) return NULL;

    char rx_buf[ADU_CAPACITY];
    const size_t expected_size = sizeof(rd_bytes_reply_header_t) + count + sizeof(crc_t);

    if(!read_impl(impl, rx_buf, expected_size)) return NULL;

    const rd_bytes_reply_t *rep = parse_reply_rd_bytes(rx_buf, expected_size);

    if(!rep) return NULL;

    memcpy(bytes, rep->bytes, count);
    return bytes + count;
}
