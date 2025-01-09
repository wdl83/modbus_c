#pragma once


typedef struct
{
    union
    {
        int fds[2];
        struct
        {
            int reader;
            int writer;
        };
    };
} pipe_t;

void pipe_reset(pipe_t *);
void pipe_open(pipe_t *, int *user_flags);
void pipe_close(pipe_t *);
