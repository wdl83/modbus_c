#pragma once

#include <stdint.h>
#include <time.h>


int64_t timestamp_ns(void);
#define timestamp_us() (timestamp_ns() / INT64_C(1000))
#define timestamp_ms() (timestamp_ns() / INT64_C(1000000))
int64_t timespec_to_us(struct timespec);
int64_t timespec_to_ms(struct timespec);
