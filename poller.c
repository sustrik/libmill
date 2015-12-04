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
#include <sys/param.h>

#include "cr.h"
#include "libmill.h"
#include "list.h"
#include "poller.h"

/* Forward declarations for the functions implemented by specific poller
   mechanisms (poll, epoll, kqueue). */
void mill_poller_init(void);
static void mill_poller_add(int fd, int events);
static void mill_poller_rm(int fd, int events);
static void mill_poller_clean(int fd);
static int mill_poller_wait(int timeout);

/* If 1, mill_poller_init was already called. */
static int mill_poller_initialised = 0;

/* Global linked list of all timers. The list is ordered.
   First timer to be resume comes first and so on. */
static struct mill_list mill_timers = {0};

/* Pause current coroutine for a specified time interval. */
void mill_msleep(int64_t deadline, const char *current) {
    mill_fdwait(-1, 0, deadline, current);
}

int mill_fdwait(int fd, int events, int64_t deadline, const char *current) {
    if(mill_slow(!mill_poller_initialised)) {
        mill_poller_init();
        mill_poller_initialised = 1;
    }
    /* If required, start waiting for the timeout. */
    if(deadline >= 0) {
        mill_running->u_fdwait.expiry = deadline;
        /* Move the timer into the right place in the ordered list
           of existing timers. TODO: This is an O(n) operation! */
        struct mill_list_item *it = mill_list_begin(&mill_timers);
        while(it) {
            struct mill_fdwait *timer = mill_cont(it, struct mill_fdwait, item);
            /* If multiple timers expire at the same momemt they will be fired
               in the order they were created in (> rather than >=). */
            if(timer->expiry > mill_running->u_fdwait.expiry)
                break;
            it = mill_list_next(it);
        }
        mill_list_insert(&mill_timers, &mill_running->u_fdwait.item, it);
    }
    /* If required, start waiting for the file descriptor. */
    if(fd >= 0)
        mill_poller_add(fd, events);
    /* Do actual waiting. */
    mill_running->state = fd < 0 ? MILL_MSLEEP : MILL_FDWAIT;
    mill_set_current(&mill_running->debug, current);
    int rc = mill_suspend();
    /* Handle file descriptor events. */
    if(rc >= 0) {
        if(deadline >= 0)
            mill_list_erase(&mill_timers, &mill_running->u_fdwait.item);
        return rc;
    }
    /* Handle the timeout. Clean-up the pollset. */
    if(fd >= 0)
        mill_poller_rm(fd, events);
    return 0;
}

void fdclean(int fd) {
    if(mill_slow(!mill_poller_initialised)) {
        mill_poller_init();
        mill_poller_initialised = 1;
    }
    mill_poller_clean(fd);
}

void mill_wait(int block) {
    if(mill_slow(!mill_poller_initialised)) {
        mill_poller_init();
        mill_poller_initialised = 1;
    }
    while(1) {
        /* Compute timeout for the subsequent poll. */
        int timeout;
        if(!block) {
            timeout = 0;
        }
        else {
            /* Compute the time till next expired sleeping coroutine. */
            if(!mill_list_empty(&mill_timers)) {
                int64_t nw = now();
                int64_t expiry = mill_cont(mill_list_begin(&mill_timers),
                    struct mill_fdwait, item)->expiry;
                timeout = nw >= expiry ? 0 : expiry - nw;
            }
            else {
                timeout = -1;
            }
        }
        /* Wait for events. */
        int fired = mill_poller_wait(timeout);
        /* Fire all expired timers. */
        if(!mill_list_empty(&mill_timers)) {
            int64_t nw = now();
            while(!mill_list_empty(&mill_timers)) {
                struct mill_fdwait *timer = mill_cont(
                    mill_list_begin(&mill_timers), struct mill_fdwait, item);
                if(timer->expiry > nw)
                    break;
                mill_list_erase(&mill_timers, mill_list_begin(&mill_timers));
                mill_resume(mill_cont(timer, struct mill_cr, u_fdwait), -1);
                fired = 1;
            }
        }
        /* Never retry the poll in non-blocking mode. */
        if(!block || fired)
            break;
        /* If timeout was hit but there were no expired timers do the poll
           again. This should not happen in theory but let's be ready for the
           case when the system timers are not precise. */
    }
}

/* Include the poll-mechanism-specific stuff. */

/* User overloads. */
#if defined MILL_EPOLL
#include "epoll.inc"
#elif defined MILL_KQUEUE
#include "kqueue.inc"
#elif defined MILL_POLL
#include "poll.inc"
/* Defaults. */
#elif defined __linux__
#include "epoll.inc"
#elif defined BSD
#include "kqueue.inc"
#else
#include "poll.inc"
#endif

