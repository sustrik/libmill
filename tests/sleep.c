
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include "../mill.h"

static uint64_t now() {
    struct timeval tv;
    int rc = gettimeofday(&tv, NULL);
    assert(rc == 0);
    return ((uint64_t)tv.tv_sec) * 1000 + (((uint64_t)tv.tv_usec) / 1000);
}

int main() {
    /* Test msleep. */
    uint64_t ms = now();
    msleep(1);
    ms = now() - ms;
    assert(ms > 900 && ms < 1100);

    /* Test musleep. */
    ms = now();
    musleep(1000000);
    ms = now() - ms;
    assert(ms > 900 && ms < 1100);

    return 0;
}

