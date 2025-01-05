#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>

#include "check.h"
#include "gnu.h"
#include "log.h"
#include "tty_pair.h"
#include "util.h"


void tty_pair_init(tty_pair_t *pair)
{
    CHECK(pair);
    memset(pair, 0, sizeof(tty_pair_t));
    pair->master_fd = -1;
}

void tty_pair_deinit(tty_pair_t *pair)
{
    if(!pair) return;
    if(pair->slave_path) FREE(pair->slave_path);
    pair->master_fd = -1;
}

void tty_pair_create(tty_pair_t *pair, const char *multiplexor, int *user_flags)
{
    CHECK(pair);
    CHECK(-1 == pair->master_fd);
    CHECK(!pair->slave_path);
    CHECK(multiplexor);

    const int flags = user_flags ? *user_flags : O_RDWR | O_NONBLOCK;

    pair->master_fd = open(multiplexor, flags);
    CHECK_ERRNO(0 == gnu_grantpt(pair->master_fd));
    CHECK_ERRNO(0 == gnu_unlockpt(pair->master_fd));

    // slave device path
    char spath[PATH_MAX];
    CHECK(0 == gnu_ptsname_r(pair->master_fd, spath, sizeof(spath)));
    CHECK_ERRNO((pair->slave_path = strdup(spath)));
    logT("master %d slave %s miltiplexor %s", pair->master_fd, pair->slave_path, multiplexor);
}
