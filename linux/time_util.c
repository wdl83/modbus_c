#include "time_util.h"

int64_t timestamp_ns(void)
{
    struct timespec ts;

    (void)clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * INT64_C(1000000000) + (int64_t)ts.tv_nsec;
}

int64_t timespec_to_us(struct timespec value)
{
    return (int64_t)value.tv_sec * INT64_C(1000000)
        + (int64_t)value.tv_nsec / INT64_C(1000);
}

int64_t timespec_to_ms(struct timespec value)
{
    return timespec_to_us(value) / INT64_C(1000);
}
