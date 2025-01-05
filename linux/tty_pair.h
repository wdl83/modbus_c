#pragma once

#define TTY_DEFAULT_MULTIPLEXOR "/dev/ptmx"


typedef struct tty_pair
{
    int master_fd;
    // must be deallocated with free()
    char *slave_path;
} tty_pair_t;

void tty_pair_init(tty_pair_t *);
void tty_pair_deinit(tty_pair_t *t);
void tty_pair_create(tty_pair_t *, const char *multiplexor, int *user_flags);
