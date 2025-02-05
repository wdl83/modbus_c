#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "check.h"
#include "util.h"

void set_signal_handler(int sig_no, signal_handler_t handler, struct sigaction *backup)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_handler = handler;
    action.sa_flags = 0;
    CHECK_ERRNO(0 == sigaction(sig_no, &action, backup));
}

void restore_signal_handler(int sig_no, struct sigaction *action)
{
    if(!action) return;
    CHECK_ERRNO(0 == sigaction(sig_no, action, NULL));
}


size_t hexdump(
    char *dst, const size_t capacity,
    const void *src, size_t size)
{
    const char lookup[] =
    {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };

    const uint8_t *src_begin = src;
    const uint8_t *const src_end = src_begin + size;
    char *begin = dst;
    const char *const end = begin + capacity;

    while(src_begin != src_end && begin != end)
    {
        *begin++ = lookup[*src_begin >> 4];
        *begin++ = lookup[*src_begin & UINT8_C(0x0F)];
        ++src_begin;
    }
    return (src_begin - (const uint8_t *)src) << 1;
}

void dump_to_file(const char *name, const void *data, size_t size)
{
    assert(name);
    assert(data);
    assert(size);
    if(!name || !data || 0u == size) return;

    FILE *file = fopen(name, "w");

    assert(file);
    if(!file) return;

    if(file)
    {
        size_t written = fwrite(data, 1, size, file);
        assert(written == size);
        fclose(file);
        file = NULL;
    }
}
