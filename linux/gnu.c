#define _GNU_SOURCE

#include <unistd.h>

#include "gnu.h"

int get_thread_id()
{
    return (int)gettid();
}
