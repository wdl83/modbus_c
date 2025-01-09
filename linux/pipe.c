#include <fcntl.h>
#include <unistd.h>

#include "check.h"
#include "gnu.h"
#include "pipe.h"


STATIC_ASSERT(sizeof(pipe_t) == sizeof(int) + sizeof(int), "check pipe_t alignment");

void pipe_reset(pipe_t *p)
{
    if(!p) return;
    p->reader = -1;
    p->writer = -1;
}

void pipe_open(pipe_t *p,  int *user_flags)
{
    if(!p) return;
    CHECK_ERRNO(!gnu_pipe2(p->fds, user_flags ? *user_flags : O_CLOEXEC));
}

void pipe_close(pipe_t *p)
{
    if(!p) return;
    if(-1 != p->reader) CHECK_ERRNO(!close(p->reader));
    p->reader = -1;
    if(-1 != p->writer) CHECK_ERRNO(!close(p->writer));
    p->writer = -1;
}
