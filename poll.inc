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

#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include "cr.h"
#include "list.h"
#include "utils.h"

/* Pollset used for waiting for file descriptors. */
static int mill_pollset_size = 0;
static int mill_pollset_capacity = 0;
static struct pollfd *mill_pollset_fds = NULL;

/* The item at a specific index in this array corresponds to the entry
   in mill_pollset fds with the same index. */
struct mill_pollset_item {
    struct mill_cr *in;
    struct mill_cr *out;
};
static struct mill_pollset_item *mill_pollset_items = NULL;

/* Find pollset index by fd. If fd is not in pollset, return the index after
   the last item. TODO: This is O(n) operation! */
static int mill_find_pollset(int fd) {
    int i;
    for(i = 0; i != mill_pollset_size; ++i) {
        if(mill_pollset_fds[i].fd == fd)
            break;
    }
    return i;
}

void mill_poller_init(void) {
    errno = 0;
}

void mill_poller_postfork(void) {
    mill_pollset_size = 0;
    mill_pollset_capacity = 0;
    mill_pollset_fds = NULL;
    mill_pollset_items = NULL;
}

static void mill_poller_add(int fd, int events) {
    int i = mill_find_pollset(fd);
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
        mill_pollset_items[i].in = mill_running;
    }
    if(events & FDW_OUT) {
        if(mill_slow(mill_pollset_items[i].out))
            mill_panic(
                "multiple coroutines waiting for a single file descriptor");
        mill_pollset_fds[i].events |= POLLOUT;
        mill_pollset_items[i].out = mill_running;
    }
}

static void mill_poller_rm(struct mill_cr *cr) {
    mill_assert(cr->fd != -1);
    int i = mill_find_pollset(cr->fd);
    mill_assert(i < mill_pollset_size);
    if(mill_pollset_items[i].in == cr) {
        mill_pollset_items[i].in = NULL;
        mill_pollset_fds[i].events &= ~POLLIN;
        cr->fd = -1;
    }
    if(mill_pollset_items[i].out == cr) {
        mill_pollset_items[i].out = NULL;
        mill_pollset_fds[i].events &= ~POLLOUT;
        cr->fd = -1;
    }
    if(!mill_pollset_fds[i].events) {
        --mill_pollset_size;
        if(i < mill_pollset_size) {
            mill_pollset_items[i] = mill_pollset_items[mill_pollset_size];
            mill_pollset_fds[i] = mill_pollset_fds[mill_pollset_size];
        }
    }
}

static void mill_poller_clean(int fd) {
}

static int mill_poller_wait(int timeout) {
    /* Wait for events. */
    int numevs;
    while(1) {
        numevs = poll(mill_pollset_fds, mill_pollset_size, timeout);
        if(numevs < 0 && errno == EINTR)
            continue;
        mill_assert(numevs >= 0);
        break;  
    }
    /* Fire file descriptor events. */
    int result = numevs > 0 ? 1 : 0;
    int i;
    for(i = 0; i < mill_pollset_size && numevs; ++i) {
        int inevents = 0;
        int outevents = 0;
        if (!mill_pollset_fds[i].revents)
            continue;
        /* Set the result values. */
        if(mill_pollset_fds[i].revents & POLLIN)
            inevents |= FDW_IN;
        if(mill_pollset_fds[i].revents & POLLOUT)
            outevents |= FDW_OUT;
        if(mill_pollset_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            inevents |= FDW_ERR;
            outevents |= FDW_ERR;
        }
        mill_pollset_fds[i].revents = 0;
        /* Resume the blocked coroutines. */
        if(mill_pollset_items[i].in &&
              mill_pollset_items[i].in == mill_pollset_items[i].out) {
            struct mill_cr *cr = mill_pollset_items[i].in;
            cr->fd = -1;
            mill_resume(cr, inevents | outevents);
            mill_pollset_fds[i].events = 0;
            mill_pollset_items[i].in = NULL;
            mill_pollset_items[i].out = NULL;
            if(mill_timer_enabled(&cr->timer))
                mill_timer_rm(&cr->timer);
        }
        else {
            if(mill_pollset_items[i].in && inevents) {
                struct mill_cr *cr = mill_pollset_items[i].in;
                cr->fd = -1;
                mill_resume(cr, inevents);
                mill_pollset_fds[i].events &= ~POLLIN;
                mill_pollset_items[i].in = NULL;
                if(mill_timer_enabled(&cr->timer))
                    mill_timer_rm(&cr->timer);
            }
            if(mill_pollset_items[i].out && outevents) {
                struct mill_cr *cr = mill_pollset_items[i].out;
                cr->fd = -1;
                mill_resume(cr, outevents);
                mill_pollset_fds[i].events &= ~POLLOUT;
                mill_pollset_items[i].out = NULL;
                if(mill_timer_enabled(&cr->timer))
                    mill_timer_rm(&cr->timer);
            }
        }
        /* If nobody is polling for the fd remove it from the pollset. */
        if(!mill_pollset_fds[i].events) {
            mill_assert(!mill_pollset_items[i].in &&
                !mill_pollset_items[i].out);
            --mill_pollset_size;
            if(i < mill_pollset_size) {
                mill_pollset_fds[i] = mill_pollset_fds[mill_pollset_size];
                mill_pollset_items[i] = mill_pollset_items[mill_pollset_size];
            }
            --i;
        }
        --numevs;
    }
    return result;
}

