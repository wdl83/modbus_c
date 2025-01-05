#pragma once

#include <stdint.h>

#include "log.h"
#include "rtu.h"

const char *rtu_state_str(uint8_t);

#define RTU_LOG_TP() logI("")
#define RTU_LOG_ERROR(str, status) \
   do { \
       logE( \
           "%s [%s|%s|U:%d|E:%d]", \
           str, \
           rtu_state_str(status.bits.prev), rtu_state_str(status.bits.curr), \
           status.bits.updated, status.bits.error); \
   } while(0)
#define RTU_LOG_EVENT(str, status) \
   do { \
       logD( \
           "%s [%s|%s|E:%d]", \
           str, \
           rtu_state_str(status.bits.prev), rtu_state_str(status.bits.curr), \
           status.bits.error); \
   } while(0)
#define RTU_LOG_DBG8(str, value) logD("%s %02X", str, (uint8_t)(value))
#define RTU_LOG_DBG16(str, value) logD("%s %04X", str, (uint16_t)(value))
