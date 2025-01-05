
#include "rtu_log_impl.h"
#include "util.h"
#include <stdint.h>


const char *rtu_state_str(uint8_t status)
{
    const char *str[] = {"INIT", "IDLE", "SOF", "RECV", "EOF", "BUSY"};
    return length_of(str) > status ? str[status] : "?";
}
