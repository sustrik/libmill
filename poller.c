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
#include "list.h"
#include "poller.h"
#include "utils.h"

#include <assert.h>
#include <poll.h>
#include <stddef.h>
#include <stdlib.h>

static const int64_t mill_bit64 = ((int64_t)1) << 63;

/* Global linked list of all timers. The list is ordered.
   First timer to be resume comes first and so on. */
static struct mill_list mill_timers = {0};

/* Pause current coroutine for a specified time interval. */
void mill_msleep(int64_t ms, const char *current) {
    /* No point in waiting. Still, let's give other coroutines a chance. */
    if(ms <= 0) {
        yield();
        return;
    }
    /* Do the actual waiting. */
    mill_fdwait(-1, 0, &ms, current);
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

/* Wait for events from a file descriptor, with an optional timeout. */
int mill_fdwait(int fd, int events, int64_t *timeout, const char *current) {
    /* If required, start waiting for the timeout. */
    if(timeout) {
        /* Compute at which point of time will the timer expire. */
        if(*timeout & mill_bit64) {
            mill_running->u_fdwait.expiry = *timeout & ~mill_bit64;
        }
        else {
            mill_running->u_fdwait.expiry = now() + *timeout;
            *timeout = mill_running->u_fdwait.expiry | mill_bit64;
        }
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
    int i;
    if(fd >= 0) {
        /* Find the fd in the pollset. TODO: This is O(n) operation! */
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
        /* Register the new file descriptor in the pollset. */
        if(events & FDW_IN) {
            if(mill_slow(mill_pollset_items[i].in))
                mill_panic(
                    "multiple coroutines waiting for a single file descriptor");
            mill_pollset_fds[i].events |= POLLIN;
            mill_pollset_items[i].in = &mill_running->u_fdwait;
        }
        if(events & FDW_OUT) {
            if(mill_slow(mill_pollset_items[i].out))
                mill_panic(
                    "multiple coroutines waiting for a single file descriptor");
            mill_pollset_fds[i].events |= POLLOUT;
            mill_pollset_items[i].out = &mill_running->u_fdwait;
        }
    }
    /* Do actual waiting. */
    mill_running->state = fd < 0 ? MILL_MSLEEP : MILL_FDWAIT;
    mill_set_current(&mill_running->debug, current);
    int rc = mill_suspend();
    /* Handle file descriptor events. */
    if(rc >= 0) {
        if(timeout)
            mill_list_erase(&mill_timers, &mill_running->u_fdwait.item);
        return rc;
    }
    /* Handle the timeout. Clean-up the pollset. */
    if(fd >= 0) {
        if(mill_pollset_items[i].in == &mill_running->u_fdwait) {
            mill_pollset_items[i].in = NULL;
            mill_pollset_fds[i].events &= ~POLLIN;
        }
        if(mill_pollset_items[i].out == &mill_running->u_fdwait) {
            mill_pollset_items[i].out = NULL;
            mill_pollset_fds[i].events &= ~POLLOUT;
        }
        if(!mill_pollset_fds[i].events) {
            --mill_pollset_size;
            if(i < mill_pollset_size) {
                mill_pollset_items[i] = mill_pollset_items[mill_pollset_size];
                mill_pollset_fds[i] = mill_pollset_fds[mill_pollset_size];
            }
        }
    }
    return 0;
}

void mill_wait(void) {
    /* The execution of the entire process would block. Let's panic. */
    if(mill_slow(mill_list_empty(&mill_timers) && !mill_pollset_size))
        mill_panic("global hang-up");

    int fired = 0;
    int rc;
    while(1) {
        /* Compute the time till next expired sleeping coroutine. */
        int timeout = -1;
        if(!mill_list_empty(&mill_timers)) {
            int64_t nw = now();
            int64_t expiry = mill_cont(mill_list_begin(&mill_timers),
                struct mill_fdwait, item)->expiry;
            timeout = nw >= expiry ? 0 : expiry - nw;
        }

        /* Wait for events. */
        rc = poll(mill_pollset_fds, mill_pollset_size, timeout);
        assert(rc >= 0);

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
                struct mill_cr, u_fdwait);
            mill_resume(cr, inevents | outevents);
            mill_pollset_fds[i].events = 0;
            mill_pollset_items[i].in = NULL;
            mill_pollset_items[i].out = NULL;
        }
        else {
            if(mill_pollset_items[i].in && inevents) {
                struct mill_cr *cr = mill_cont(mill_pollset_items[i].in,
                    struct mill_cr, u_fdwait);
                mill_resume(cr, inevents);
                mill_pollset_fds[i].events &= ~POLLIN;
                mill_pollset_items[i].in = NULL;
            }
            else if(mill_pollset_items[i].out && outevents) {
                struct mill_cr *cr = mill_cont(mill_pollset_items[i].out,
                    struct mill_cr, u_fdwait);
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

