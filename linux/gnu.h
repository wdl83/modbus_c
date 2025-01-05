#pragma once

// warppers around GNU/XOPEN/POSIX extensions

int gnu_thread_id(void);
int gnu_grantpt(int fd);
int gnu_unlockpt(int fd);
int gnu_ptsname_r(int fd, char *buf, size_t buflen);
