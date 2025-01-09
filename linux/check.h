#pragma once

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

#define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)

#define STATIC_ASSERT_STRUCT_OFFSET(type, field, offset) \
    STATIC_ASSERT( \
        offsetof(type, field) == (size_t)(offset), \
        "member " #field " of " #type ", offset " #offset " mismatch")

#define STRINGIFY(what) #what

#ifdef NDEBUG

#define ABORT(hint) \
    do { \
        logA("aborting: %s", STRINGIFY(hint)); \
        abort(); \
    } while(0)

#else // NDEBUG

#define ABORT(hint) assert(0 && STRINGIFY(hint))

#endif // NDEBUG

#define CHECK(cond) \
    do { \
        if(cond) break; \
        ABORT(cond); \
    } while(0)

#define CHECK_ERRNO2(cond, errno_) \
    do { \
        if(cond) break; \
        logE(STRINGIFY(cond) " %s(%d)", strerror(errno_), errno_); \
        ABORT(STRINGIFY(cond)); \
    } while(0)

#define CHECK_ERRNO(cond) CHECK_ERRNO2(cond, errno)

#define CHECK_ERRNO_GOTO(cond, label) \
    do { \
        if(cond) break; \
        logE(STRINGIFY(cond) " %s(%d)", strerror(errno), errno); \
        goto label; \
    } while(0)

#define CHECK_ERRNO_RETURN(cond, result) \
    do { \
        if(cond) break; \
        logE(STRINGIFY(cond) " %s(%d)", strerror(errno), errno); \
        return result; \
    } while(0)
