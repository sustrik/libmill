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

#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#if defined __APPLE__
#include <mach/mach_time.h>
static mach_timebase_info_data_t mill_mtid = {0};
#endif

#include "libmill.h"
#include "timer.h"
#include "utils.h"

/* 1 millisecond expressed in CPU ticks. The value is chosen is such a way that
   it works reasonably well for CPU frequencies above 500MHz. On significanly
   slower machines you may wish to reconsider. */
#define MILL_CLOCK_PRECISION 1000000

/* Returns current time by querying the operating system. */
static int64_t mill_os_time(void) {
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

int64_t mill_now_(void) {
#if (defined __GNUC__ || defined __clang__) && \
      (defined __i386__ || defined __x86_64__)
    /* Get the timestamp counter. This is time since startup, expressed in CPU
       cycles. Unlike gettimeofday() or similar function, it's extremely fast -
       it takes only few CPU cycles to evaluate. */
    uint32_t low;
    uint32_t high;
    __asm__ volatile("rdtsc" : "=a" (low), "=d" (high));
    int64_t tsc = (int64_t)((uint64_t)high << 32 | low);
    /* These global variables are used to hold the last seen timestamp counter
       and last seen time measurement. We'll initilise them the first time
       this function is called. */
    static int64_t last_tsc = -1;
    static int64_t last_now = -1;
    if(mill_slow(last_tsc < 0)) {
        last_tsc = tsc;
        last_now = mill_os_time();
    }   
    /* If TSC haven't jumped back or progressed more than 1/2 ms, we can use
       the cached time value. */
    if(mill_fast(tsc - last_tsc <= (MILL_CLOCK_PRECISION / 2) &&
          tsc >= last_tsc))
        return last_now;
    /* It's more than 1/2 ms since we've last measured the time.
       We'll do a new measurement now. */
    last_tsc = tsc;
    last_now = mill_os_time();
    return last_now;
#else
    return mill_os_time();
#endif
}

/* Global linked list of all timers. The list is ordered.
   First timer to be resume comes first and so on. */
static struct mill_list mill_timers = {0};

void mill_timer_add(struct mill_timer *timer, int64_t deadline,
      mill_timer_callback callback) {
    mill_assert(deadline >= 0);
    timer->expiry = deadline;
    timer->callback = callback;
    /* Move the timer into the right place in the ordered list
       of existing timers. TODO: This is an O(n) operation! */
    struct mill_list_item *it = mill_list_begin(&mill_timers);
    while(it) {
        struct mill_timer *tm = mill_cont(it, struct mill_timer, item);
        /* If multiple timers expire at the same momemt they will be fired
           in the order they were created in (> rather than >=). */
        if(tm->expiry > timer->expiry)
            break;
        it = mill_list_next(it);
    }
    mill_list_insert(&mill_timers, &timer->item, it);
}

void mill_timer_rm(struct mill_timer *timer) {
    mill_assert(timer->expiry >= 0);
    mill_list_erase(&mill_timers, &timer->item);
    timer->expiry = -1;
}

int mill_timer_next(void) {
    if(mill_list_empty(&mill_timers))
        return -1;
    int64_t nw = now();
    int64_t expiry = mill_cont(mill_list_begin(&mill_timers),
        struct mill_timer, item)->expiry;
    return (int) (nw >= expiry ? 0 : expiry - nw);
}

int mill_timer_fire(void) {
    /* Avoid getting current time if there are no timers anyway. */
    if(mill_list_empty(&mill_timers))
        return 0;
    int64_t nw = now();
    int fired = 0;
    while(!mill_list_empty(&mill_timers)) {
        struct mill_timer *tm = mill_cont(
            mill_list_begin(&mill_timers), struct mill_timer, item);
        if(tm->expiry > nw)
            break;
        mill_list_erase(&mill_timers, mill_list_begin(&mill_timers));
        tm->expiry = -1;
        if(tm->callback)
            tm->callback(tm);
        fired = 1;
    }
    return fired;
}

void mill_timer_postfork(void) {
    mill_list_init(&mill_timers);
}

