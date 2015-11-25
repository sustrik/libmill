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

#include <string.h>
#include <sys/epoll.h>
#include <sys/resource.h>

#include "cr.h"
#include "poller.h"
#include "utils.h"

#define MILL_EPOLLSETSIZE 128

static int mill_efd = -1;

static int mill_getefd(void) {
    /* First time this function is called it creates the global pollset. */
    if(mill_slow(mill_efd == -1)) {
        mill_efd = epoll_create(1);
        mill_assert(mill_efd >= 0);
    }
    return mill_efd;
}

/* Epoll allows to register only a single pointer with a file decriptor.
   However, we may need two pointers to coroutines. One for the coroutine
   waiting to receive data from the descriptor, one for the coroutine waiting
   to send data to the descriptor. Thus, we are going to keep an array of
   pointer pairs for each file descriptor. */
struct mill_crpair {
    struct mill_fdwait *in;
    struct mill_fdwait *out;
};

static struct mill_crpair *mill_crpairs = NULL;

static struct mill_crpair *mill_getcrpair(int fd) {
    if(!mill_crpairs) {
        struct rlimit rlim;
        int rc = getrlimit(RLIMIT_NOFILE, &rlim);
        int maxfds = rlim.rlim_max;
        mill_assert(rc == 0);
        mill_crpairs = (struct mill_crpair*)
            malloc(sizeof(struct mill_crpair) * maxfds);
        mill_assert(mill_crpairs);
        memset(mill_crpairs, 0, sizeof(struct mill_crpair) * maxfds);
    }
    return &mill_crpairs[fd];
}

int mill_fdwait(int fd, int events, int64_t deadline, const char *current) {
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
    struct mill_crpair *crp = NULL;
    if(fd >= 0) {
        crp = mill_getcrpair(fd);
        int exists = crp->in || crp->out;
        struct epoll_event ev;
        ev.events = 0;
        ev.data.fd = fd;
        if(events & FDW_IN) {
            if(crp->in)
                mill_panic(
                    "multiple coroutines waiting for a single file descriptor");
            crp->in = &mill_running->u_fdwait;
            ev.events |= EPOLLIN;
        }
        if(events & FDW_OUT) {
            if(crp->out)
                mill_panic(
                    "multiple coroutines waiting for a single file descriptor");
            crp->out = &mill_running->u_fdwait;
            ev.events |= EPOLLOUT;
        }
        int rc = epoll_ctl(mill_getefd(),
            exists ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, fd, &ev);
        if(rc != 0) {
            /* TODO: Rollback the changes here. */
            mill_assert(0);
        }
    }
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
    if(fd >= 0) {
        struct epoll_event ev;
        ev.events = 0;
        ev.data.fd = fd;
        if(crp->in && crp->in != &mill_running->u_fdwait)
            ev.events |= EPOLLIN;
        else
            crp->in = NULL;
        if(crp->out && crp->out != &mill_running->u_fdwait)
            ev.events |= EPOLLIN;
        else
            crp->out = NULL;
        int rc = epoll_ctl(mill_getefd(),
            ev.events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL, fd, &ev);
        if(rc != 0) {
            /* TODO: What to do here? */
            mill_assert(0);
        }
    }
    return 0;
}

void mill_wait(int block) {
    int efd = mill_getefd();

    int fired = 0;
    int rc;
    struct epoll_event evs[MILL_EPOLLSETSIZE];
    while(1) {

        /* Compute timeout for the subsequent poll. */
        int timeout;
        if(!block) {
            timeout = 0;
        }\
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
        rc = epoll_wait(efd, evs, MILL_EPOLLSETSIZE, timeout);
        if(rc < 0 && errno == EINTR)
            continue;
        mill_assert(rc >= 0);

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
        if(!block)
            break;
        /* If timeout was hit but there were no expired timers do the poll
           again. This should not happen in theory but let's be ready for the
           case when the system timers are not precise. */
        if(!(rc == 0 && !fired))
            break;
    }

    /* Fire file descriptor events. */
    int i;
    for(i = 0; i != rc; ++i) {
        struct mill_crpair *crp = mill_getcrpair(evs[i].data.fd);
        struct epoll_event ev;
        ev.events = 0;
        ev.data.fd = evs[i].data.fd;
        int inevents = 0;
        int outevents = 0;
        /* Set the result values. */
        if(evs[i].events & EPOLLIN)
            inevents |= FDW_IN;
        else if(crp->in)
            ev.events |= EPOLLIN;
        if(evs[i].events & EPOLLOUT)
            outevents |= FDW_OUT;
        else if(crp->out)
            ev.events |= EPOLLOUT;
        if(evs[i].events & (EPOLLERR | EPOLLHUP)) {
            inevents |= FDW_ERR;
            outevents |= FDW_ERR;
            ev.events = 0;
        }
        /* Resume the blocked coroutines. */  
        int rc = epoll_ctl(efd, ev.events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL,
            evs[i].data.fd, &ev);
        mill_assert(rc == 0);
        if(crp->in == crp->out) {
            struct mill_cr *cr = mill_cont(crp->in, struct mill_cr, u_fdwait);
            mill_resume(cr, inevents | outevents);
            crp->in = NULL;
            crp->out = NULL;
        }
        else {
            if(crp->in && inevents) {
                struct mill_cr *cr = mill_cont(crp->in, struct mill_cr,
                    u_fdwait);
                mill_resume(cr, inevents);
                crp->in = NULL;
            }
            if(crp->out && outevents) {
                struct mill_cr *cr = mill_cont(crp->out, struct mill_cr,
                    u_fdwait);
                mill_resume(cr, outevents);
                crp->out = NULL;
            }
        }
    }
}

