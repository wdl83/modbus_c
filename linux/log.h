#pragma once

#include <stdio.h>

#include <unistd.h>

#include "gnu.h"

#ifndef BASE_FILE_NAME
#define BASE_FILE_NAME "?"
#endif

typedef enum
{
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE
} log_level_t;

#ifdef __cplusplus
extern "C" {
#endif

int current_log_level(void);

#ifdef __cplusplus
} // extern "C"
#endif

#define LOG_IMPL(dst, prefix, fmt, ...) \
    do { \
        fprintf( \
            dst, "[%d:%d:" prefix "] %s:%d %s " fmt "\n", \
            getpid(), gnu_thread_id(), \
            BASE_FILE_NAME, __LINE__, __func__, ##__VA_ARGS__); \
        fflush(dst); \
    } while(0)

#define LOG_IF_IMPL(dst, level, prefix, fmt, ...) \
    do { \
        if(current_log_level() < level) break; \
        LOG_IMPL(dst, prefix, fmt, ##__VA_ARGS__); \
    } while(0)

// log always with tag "A"
#define logA(fmt, ...) LOG_IMPL(stdout, "A", fmt, ##__VA_ARGS__)
#define logE(fmt, ...) LOG_IF_IMPL(stderr, 0, "E", fmt, ##__VA_ARGS__)
#define logW(fmt, ...) LOG_IF_IMPL(stderr, 1, "W", fmt, ##__VA_ARGS__)
#define logI(fmt, ...) LOG_IF_IMPL(stdout, 2, "I", fmt, ##__VA_ARGS__)
#define logD(fmt, ...) LOG_IF_IMPL(stdout, 3, "D", fmt, ##__VA_ARGS__)
#define logT(fmt, ...) LOG_IF_IMPL(stdout, 4, "T", fmt, ##__VA_ARGS__)
