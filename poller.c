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

#include "cr.h"
#include "libmill.h"
#include "model.h"
#include "poller.h"
#include "utils.h"

#include <assert.h>
#include <poll.h>
#include <stddef.h>
#include <stdlib.h>

/* Global linked list of all timers. The list is ordered.
   First timer to be resume comes first and so on. */
static struct mill_slist mill_timers = {0};

/* Pause current coroutine for a specified time interval. */
void mill_msleep(long ms, const char *current) {
    /* No point in waiting. However, let's give other coroutines a chance. */
    if(ms <= 0) {
        yield();
        return;
    }
    /* Compute at which point of time will the timer expire. */
    mill_running->sleeper.expiry = mill_now() + ms;    
    /* Move the coroutine into the right place in the ordered list
       of sleeping coroutines. */
    struct mill_slist_item *it = mill_slist_begin(&mill_timers);
    if(!it) {
        mill_slist_push(&mill_timers, &mill_running->sleeper.item);
    }
    else {
       while(it) {
           struct mill_msleep *next = mill_cont(mill_slist_next(it),
               struct mill_msleep, item);
           if(!next || next->expiry > mill_running->sleeper.expiry) {
               mill_slist_insert(&mill_timers, &mill_running->sleeper.item, it);
               break;
           }
           it = mill_slist_next(it);
       }
    }
    /* Wait while the timer expires. */
    mill_running->state = MILL_MSLEEP;
    mill_set_current(&mill_running->debug, current);
    mill_suspend();
}

/* Pollset used for waiting for file descriptors. */
static int mill_pollset_size = 0;
static int mill_pollset_capacity = 0;
static struct pollfd *mill_pollset_fds = NULL;

/* The item at a specific index in this array corresponds to the entry
   in mill_pollset fds with the same index. */
struct mill_pollset_item {
    struct mill_fdwait *in;
    struct mill_fdwait *out;
};
static struct mill_pollset_item *mill_pollset_items = NULL;

/* Wait for events from a file descriptor. */
int mill_fdwait(int fd, int events, long timeout, const char *current) {
    /* Find the fd in the pollset. TODO: This is O(n) operation! */
    int i;
    for(i = 0; i != mill_pollset_size; ++i) {
        if(mill_pollset_fds[i].fd == fd)
            break;
    }
    /* Grow the pollset as needed. */
    if(i == mill_pollset_size) { 
		if(mill_pollset_size == mill_pollset_capacity) {
		    mill_pollset_capacity = mill_pollset_capacity ?
                mill_pollset_capacity * 2 : 64;
		    mill_pollset_fds = realloc(mill_pollset_fds,
		        mill_pollset_capacity * sizeof(struct pollfd));
		    mill_pollset_items = realloc(mill_pollset_items,
		        mill_pollset_capacity * sizeof(struct mill_pollset_item));
		}
        ++mill_pollset_size;
        mill_pollset_fds[i].fd = fd;
        mill_pollset_fds[i].events = 0;
        mill_pollset_fds[i].revents = 0;
        mill_pollset_items[i].in = NULL;
        mill_pollset_items[i].out = NULL;
    }
    /* Register the new poller in the pollset. */
    if(events & FDW_IN) {
        if(mill_pollset_items[i].in)
            mill_panic(
                "multiple coroutines waiting for a single file descriptor");
        mill_pollset_fds[i].events |= POLLIN;
        mill_pollset_items[i].in = &mill_running->fdwaiter;
    }
    if(events & FDW_OUT) {
        if(mill_pollset_items[i].out)
            mill_panic(
                "multiple coroutines waiting for a single file descriptor");
        mill_pollset_fds[i].events |= POLLOUT;
        mill_pollset_items[i].out = &mill_running->fdwaiter;
    }
    /* Wait for the signal from the file descriptor. */
    mill_running->state = MILL_FDWAIT;
    mill_set_current(&mill_running->debug, current);
    return mill_suspend();
}

void mill_wait(void) {
    /* The execution of the entire process would block. Let's panic. */
    if(mill_slist_empty(&mill_timers) && !mill_pollset_size)
        mill_panic("global hang-up");

    int fired = 0;
    int rc;
    while(1) {
        /* Compute the time till next expired sleeping coroutine. */
        int timeout = -1;
        if(!mill_slist_empty(&mill_timers)) {
            uint64_t nw = mill_now();
            uint64_t expiry = mill_cont(mill_slist_begin(&mill_timers),
                struct mill_msleep, item)->expiry;
            timeout = nw >= expiry ? 0 : expiry - nw;
        }

        /* Wait for events. */
        rc = poll(mill_pollset_fds, mill_pollset_size, timeout);
        assert(rc >= 0);

        /* Fire all expired timers. */
        if(!mill_slist_empty(&mill_timers)) {
            uint64_t nw = mill_now();
            while(!mill_slist_empty(&mill_timers)) {
                struct mill_msleep *timer = mill_cont(
                    mill_slist_begin(&mill_timers), struct mill_msleep, item);
                if(timer->expiry > nw)
                    break;
                mill_slist_pop(&mill_timers);
                mill_resume(mill_cont(timer, struct mill_cr, sleeper), 0);
                fired = 1;
            }
        }

        /* If timeout was hit but there were no expired timers do the poll
           again. This should not happen in theory but let's be ready for the
           case when the system timers are not precise. */
        if(!(rc == 0 && !fired))
            break;
    }

    /* Fire file descriptor events. */
    int i;
    for(i = 0; i != mill_pollset_size && rc; ++i) {
        int inevents = 0;
        int outevents = 0;
        /* Set the result values. */
        if(mill_pollset_fds[i].revents & POLLIN)
            inevents |= FDW_IN;
        if(mill_pollset_fds[i].revents & POLLOUT)
            outevents |= FDW_OUT;
        if(mill_pollset_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            inevents |= FDW_ERR;
            outevents |= FDW_ERR;
        }
        /* Fire the callbacks. */
        if(mill_pollset_items[i].in &&
              mill_pollset_items[i].in == mill_pollset_items[i].out) {
            struct mill_cr *cr = mill_cont(mill_pollset_items[i].in,
                struct mill_cr, fdwaiter);
            mill_resume(cr, inevents | outevents);
            mill_pollset_fds[i].events = 0;
            mill_pollset_items[i].in = NULL;
            mill_pollset_items[i].out = NULL;
        }
        else {
            if(mill_pollset_items[i].in && inevents) {
                struct mill_cr *cr = mill_cont(mill_pollset_items[i].in,
                    struct mill_cr, fdwaiter);
                mill_resume(cr, inevents);
                mill_pollset_fds[i].events &= ~POLLIN;
                mill_pollset_items[i].in = NULL;
            }
            else if(mill_pollset_items[i].out && outevents) {
                struct mill_cr *cr = mill_cont(mill_pollset_items[i].out,
                    struct mill_cr, fdwaiter);
                mill_resume(cr, outevents);
                mill_pollset_fds[i].events &= ~POLLOUT;
                mill_pollset_items[i].out = NULL;
            }
        }
        /* If nobody is polling for the fd remove it from the pollset. */
        if(!mill_pollset_fds[i].events) {
            assert(!mill_pollset_items[i].in && !mill_pollset_items[i].out);
            mill_pollset_fds[i] = mill_pollset_fds[mill_pollset_size - 1];
            mill_pollset_items[i] = mill_pollset_items[mill_pollset_size - 1];
            --mill_pollset_size;
            --rc;
        }
    }
}

