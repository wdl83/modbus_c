#pragma once


#ifdef RTU_LOG_DISABLED

#define RTU_LOG_TP()
// status: modbus_rtu_status_t
#define RTU_LOG_ERROR(str, status)
#define RTU_LOG_EVENT(str, status)
#define RTU_LOG_DBG8(str, value)
#define RTU_LOG_DBG16(str, value)

#else

#include "rtu_log_impl.h"

#endif
