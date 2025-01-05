#pragma once

#include <assert.h>
#include <stdlib.h>

#include "check.h"

#define min(a, b)                                        ((a) < (b) ? (a) : (b))
#define max(a, b)                                        ((a) > (b) ? (a) : (b))

#define length_of(array)                      (sizeof(array) / sizeof(array[0]))
#define null_field(type, field)                          (((type *)NULL)->field)
#define sizeof_field(type, field)                sizeof(null_field(type, field))
#define length_of_field(type, field) \
    (sizeof_field(type, field) / sizeof(null_field(type, field)[0]))

#define FREE(p) \
    do { \
        if(!(p)) break; \
        free((void *)(p)); \
        p = NULL; \
    } while(0)

#ifdef __cplusplus
extern "C" {
#endif

size_t hexdump(char *dst, size_t capacity, const void *what, size_t size);
void dump_to_file(const char *name, const void *data, size_t size);
#ifdef __cplusplus
} // extern "C"
#endif
