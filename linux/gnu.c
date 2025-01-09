#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>

#include "gnu.h"

int gnu_thread_id() { return (int)gettid(); }
int gnu_grantpt(int fd) { return grantpt(fd); }
int gnu_unlockpt(int fd) { return unlockpt(fd); }
int gnu_ptsname_r(int fd, char *buf, size_t buflen) { return ptsname_r(fd, buf, buflen); }
int gnu_pipe2(int pipefd[2], int flags) { return pipe2(pipefd, flags); }
