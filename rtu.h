#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * MODBUS over serial line specification and implementation guide V1.02
 *
 * In RTU mode, message frames are separated by a silent interval of at least
 * 3.5 character times. In the following sections, this time interval
 * is called t3,5.
 * [...]
 * The entire message frame must be transmitted as a continuous stream
 * of characters. If a silent interval of more than 1.5 character times occurs
 * between two characters, the message frame is declared incomplete and
 * should be discarded by the receiver.
 * [...]
 * Remark :
 * The implementation of RTU reception driver may imply the management
 * of a lot of interruptions due to the t 1.5 and t 3.5 timers. With
 * high communication baud rates, this leads to a heavy CPU load.
 * Consequently these two timers must be strictly respected when the
 * baud rate is equal or lower than 19200 Bps. For baud rates greater
 * than 19200 Bps, fixed values for the 2 timers should be used: it is
 * recommended to use a value of 750μs for the inter-character time-out (t 1.5 )
 * and a value of 1.750ms for inter-frame delay (t 3.5 ).
 * */

/*
 * Silent Interval ...[FRAME1]...[FRAME2]<-... min 3.5t ...->[FRAME3]...
 *
 * Inter Frame Timeout [N0, N1, ... Ni, <-... max 1.5t ... -> Nj, ... Nn]
 *
 *  11bits / frame (4-bits per character)
 * even parity:
 * [start_bit | 8_data_bits | parity_bit | stop_bit]
 * OR (if parity disabled):
 * [start_bit | 8_data_bits | stop_bit   | stop_bit]
 *
 * Example:
 * 9600bps === (872 * 2)c / 10^6us == 1744c / 10^6us
 *
 * 1.5t = 1.5c * (10^6us / 1744c)
 *      = 15c * (10^5us / 1744c)
 *      = 15c * 57us/c
 *      = 855us
 *
 * 19200bps === (1745 * 2)c / 10^6us = 3490c / 10^6us
 * 1.5t = 1.5c * (10^6us / 3490c)
 *      = 15c * (10^5us / 3490c)
 *      = 15c * 28us/c
 *      = 420us
 */


/* characters per second */
#define CALC_CPS(BR) ((BR) / 10)
#define CALC_MAX_INTER_FRAME_TIMEOUT_us(BR) (15 * (100000 / CALC_CPS(BR)))
#define CALC_MIN_SILENT_INTERVAL_us(BR)

/* Function Codes
 *
 * Invalid: 0
 * Public: 1..65
 * User: 65..72
 * Public: 73..99
 * User: 100..110
 * Public: 111...127
 * 128..255 reserved for exception responses */

#define FCODE_INVALID 0

/* <-- 1-bit Physical Discrete Inputs */
#define FCODE_RD_INPUT 2
/* --> 1-bit Physical Discrete Inputs */

/* <-- 1-bit Internal Bits or Physical coils */
#define FCODE_RD_COIL 1
#define FCODE_WR_COIL 5
#define FCODE_WR_COILS 15
/* --> 1-bit Internal Bits or Physical coils */

/* <-- 16-bit Physical Input Registers */
#define FCODE_RD_IN_REGISTER 4
/* --> Physical Input Registers */

/* <-- 16bit Internal Registers of Physical Output Registers */
#define FCODE_RD_HOLDING_REGISTERS 3
#define FCODE_WR_REGISTER 6
#define FCODE_WR_REGISTERS 16
#define FCODE_RD_WR_REGISTERS 23
#define FCODE_MWR_REGISTER 22
#define FCODE_RD_FIFO 24
/* --> 16-bit Internal Registers of Physical Output Registers */

/* <-- File Record Access */
#define FCODE_RD_FILE 20
#define FCODE_WR_FILE 21
/* --> File Record Access */

/* <-- Diagnostics */
#define FCODE_RD_EXCEPTION_STATUS  7
#define FCODE_DIAGNOSTIC  8
#define FCODE_GET_COM_EVENT_CNTR 11
#define FCODE_GET_COM_EVENT_LOG 12
#define FCODE_REPORT_SERVER_ID 17
#define FCODE_RD_DEVICE_ID 43
/* --> Diagnostics */

/* <-- User1 (65..72) */
#define FCODE_USER1_OFFSET 65

/* byte-oriented functions - similar to standard MODBUS
 * register rd/wr function but dedicated for devices with
 * 16-bit byte-addressable space */

/* REQUEST:
 * | 2xbyte (src address)           |
 * | 1xbyte (number of data bytes)  |
 * REPLY:
 * | 2xbyte (src address)           |
 * | 1xbyte (number of data bytes)  |
 * | 1xbyte (data[0])               |
 * | ...                            |
 * | 1xbyte (data[N-1])             | */
#define FCODE_RD_BYTES (FCODE_USER1_OFFSET + 0)
/* REQUEST:
 * | 2xbyte (dst address)           |
 * | 1xbyte (number of data bytes)  |
 * | 1xbyte (data[0])               |
 * | ...                            |
 * | 1xbyte (data[N-1])             |
 * REPLY:
 * | 2xbyte (dst address)           |
 * | 1xbyte (number of data bytes)  | */
#define FCODE_WR_BYTES (FCODE_USER1_OFFSET + 1)
/* --> User */

/* Excption codes */
#define ECODE_ILLEGAL_FUNCTION 0x1
#define ECODE_ILLEGAL_DATA_ADDRESS 0x2
#define ECODE_ILLEGAL_DATA_VALUE 0x3
#define ECODE_SERVER_DEVICE_FAILURE 0x4

#define ECODE_USER_OFFSET 0x80
#define ECODE_FORMAT_ERROR (ECODE_USER_OFFSET + 0)

/*
 * [Additional address] [Function code] [Data] [Error check]
 *
 * <-ADU--------------------------------------------------->
 *                      <-PDU---------------->
 */

#define ADU_CAPACITY 256
#define ADU_MIN_SIZE (1 /* ADDR */ + 1 /* FCODE */ + 2 /* CRC */)
#define PDU_CAPACITY 253
#define PDU_DATA_CAPACITY 252

typedef uint8_t modbus_rtu_addr_t;
typedef uint8_t modbus_rtu_fcode_t;
typedef uint16_t modbus_rtu_crc_t;
typedef uint8_t modbus_rtu_ecode_t; /* exception */

#define BROADCAST_ADDR 0

struct modbus_rtu_state;
typedef struct modbus_rtu_state modbus_rtu_state_t;

typedef
void (*modbus_rtu_timer_cb_t)(modbus_rtu_state_t *);

typedef
void (*modbus_rtu_timer_start_t)(modbus_rtu_state_t *);

typedef
void (*modbus_rtu_timer_stop_t)(modbus_rtu_state_t *);

typedef
void (*modbus_rtu_timer_reset_t)(modbus_rtu_state_t *);

/* callback on every byte received on serial line */
typedef
void (*modbus_rtu_serial_recv_cb_t)(modbus_rtu_state_t *, uint8_t data);

/* callback on any error on serial line */
typedef
void (*modbus_rtu_serial_recv_err_cb_t)(modbus_rtu_state_t *, uint8_t data);

typedef
void (*modbus_rtu_serial_sent_cb_t)(modbus_rtu_state_t *);

typedef
void (*modbus_rtu_serial_send_t)(modbus_rtu_state_t *, modbus_rtu_serial_sent_cb_t);

typedef
uint8_t *(*modbus_rtu_pdu_cb_t)(
    modbus_rtu_state_t *,
    modbus_rtu_addr_t,
    modbus_rtu_fcode_t,
    const uint8_t *begin, const uint8_t *end,
    const uint8_t *curr,
    uint8_t *dst_begin, const uint8_t *const dst_end,
    uintptr_t user_data);

typedef
void (*modbus_rtu_suspend_cb_t)(uintptr_t user_data);
typedef
void (*modbus_rtu_resume_cb_t)(uintptr_t user_data);

enum
{
    RTU_STATE_INIT,
    RTU_STATE_IDLE,
    RTU_STATE_SOF,
    RTU_STATE_RECV,
    RTU_STATE_EOF,
    /* transmission in progress */
    RTU_STATE_BUSY
};

typedef union
{
    struct
    {
        uint8_t updated : 1;
        uint8_t error : 1;
        uint8_t prev : 3;
        uint8_t curr : 3;
    };
    uint8_t value;
}  modbus_rtu_status_t;

#define RTU_STATE_UPDATE(status, state) \
    status.updated = 1; \
    status.prev = status.curr; \
    status.curr = state;

#define RTU_STATE_ERROR(status) \
    status.updated = 1; \
    status.error = 1;

#define RXBUF_CAPACITY ADU_CAPACITY
#define TXBUF_CAPACITY ADU_CAPACITY

struct modbus_rtu_state
{
    modbus_rtu_status_t status;

    modbus_rtu_addr_t addr;

    modbus_rtu_timer_start_t timer_start_1t5;
    modbus_rtu_timer_start_t timer_start_3t5;
    modbus_rtu_timer_stop_t timer_stop;
    modbus_rtu_timer_reset_t timer_reset;
    modbus_rtu_timer_cb_t timer_cb; // is set by modus state machine

    modbus_rtu_serial_recv_cb_t serial_recv_cb;
    modbus_rtu_serial_recv_err_cb_t serial_recv_err_cb;

    modbus_rtu_serial_send_t serial_send;
    modbus_rtu_serial_sent_cb_t serial_sent_cb;

    modbus_rtu_pdu_cb_t pdu_cb;

    modbus_rtu_suspend_cb_t suspend_cb;
    modbus_rtu_resume_cb_t resume_cb;

    uint8_t rxbuf[RXBUF_CAPACITY];
    uint8_t *rxbuf_curr;
    uint8_t txbuf[TXBUF_CAPACITY];
    uint8_t *txbuf_curr;
    uintptr_t user_data;
    uint8_t err_cntr;
    uint8_t serial_recv_err_cntr;
    uint8_t crc_err_cntr;
};

void modbus_rtu_init(
    modbus_rtu_state_t *,
    modbus_rtu_addr_t,
    modbus_rtu_timer_start_t /* 1,5t*/,
    modbus_rtu_timer_start_t /* 3,5t */,
    modbus_rtu_timer_stop_t,
    modbus_rtu_timer_reset_t,
    modbus_rtu_serial_send_t,
    modbus_rtu_pdu_cb_t,
    modbus_rtu_suspend_cb_t,
    modbus_rtu_resume_cb_t,
    uintptr_t);

void modbus_rtu_event(modbus_rtu_state_t *);

modbus_rtu_addr_t modbus_rtu_addr(modbus_rtu_state_t *state);
bool modbus_rtu_idle(modbus_rtu_state_t *);
