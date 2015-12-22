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

#include <sys/time.h>
#include <time.h>

#if defined __APPLE__
#include <mach/mach_time.h>
static mach_timebase_info_data_t mill_mtid = {0};
#endif

#include "cr.h"
#include "libmill.h"
#include "list.h"
#include "timer.h"
#include "utils.h"

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

/* Global linked list of all timers. The list is ordered.
   First timer to be resume comes first and so on. */
static struct mill_list mill_timers = {0};

void mill_timer_add(int64_t deadline) {
    mill_assert(deadline >= 0);
    mill_running->expiry = deadline;
    /* Move the timer into the right place in the ordered list
       of existing timers. TODO: This is an O(n) operation! */
    struct mill_list_item *it = mill_list_begin(&mill_timers);
    while(it) {
        struct mill_cr *cr = mill_cont(it, struct mill_cr, timer);
        /* If multiple timers expire at the same momemt they will be fired
           in the order they were created in (> rather than >=). */
        if(cr->expiry > mill_running->expiry)
            break;
        it = mill_list_next(it);
    }
    mill_list_insert(&mill_timers, &mill_running->timer, it);
}

void mill_timer_rm(void) {
    mill_list_erase(&mill_timers, &mill_running->timer);
}

int mill_timer_next(void) {
    if(mill_list_empty(&mill_timers))
        return -1;
    int64_t nw = now();
    int64_t expiry = mill_cont(mill_list_begin(&mill_timers),
        struct mill_cr, timer)->expiry;
    return (int) (nw >= expiry ? 0 : expiry - nw);
}

int mill_timer_fire(void) {
    /* Avoid getting current time if there are no timers anyway. */
    if(mill_list_empty(&mill_timers))
        return 0;
    int64_t nw = now();
    int fired = 0;
    while(!mill_list_empty(&mill_timers)) {
        struct mill_cr *cr = mill_cont(
            mill_list_begin(&mill_timers), struct mill_cr, timer);
        if(cr->expiry > nw)
            break;
        mill_list_erase(&mill_timers, mill_list_begin(&mill_timers));
        mill_resume(cr, -1);
        fired = 1;
    }
    return fired;
}

