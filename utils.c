/*

  Copyright (c) 2015 Martin Sustrik

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "libmill.h"
#include "utils.h"

#if defined __APPLE__
#include <mach/mach_time.h>
static mach_timebase_info_data_t mill_mtid = {0};
#endif

void mill_panic(const char *text) {
    fprintf(stderr, "panic: %s\n", text);
    abort();
}

int64_t now(void) {
#if defined __APPLE__
    if (mill_slow(!mill_mtid.denom))
        mach_timebase_info(&mill_mtid);
    uint64_t ticks = mach_absolute_time();
    return (int64_t)(ticks * mill_mtid.numer / mill_mtid.denom / 1000000);
#elif defined CLOCK_MONOTONIC
    struct timespec ts;
    int rc = clock_gettime(CLOCK_MONOTONIC, &ts);
    mill_assert (rc == 0);
    return ((int64_t)ts.tv_sec) * 1000 + (((int64_t)ts.tv_nsec) / 1000000);
#else
    struct timeval tv;
    int rc = gettimeofday(&tv, NULL);
    assert(rc == 0);
    return ((int64_t)tv.tv_sec) * 1000 + (((int64_t)tv.tv_usec) / 1000);
#endif
}
