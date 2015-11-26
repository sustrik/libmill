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
#include "utils.h"

#define MILL_EPOLLSETSIZE 128

/* Global pollset. */
static int mill_efd = -1;

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

static void mill_poller_init(void) {
    mill_efd = epoll_create(1);
    mill_assert(mill_efd >= 0);
    struct rlimit rlim;
    int rc = getrlimit(RLIMIT_NOFILE, &rlim);
    int maxfds = rlim.rlim_max;
    mill_assert(rc == 0);
    mill_crpairs = (struct mill_crpair*)
        malloc(sizeof(struct mill_crpair) * maxfds);
    mill_assert(mill_crpairs);
    memset(mill_crpairs, 0, sizeof(struct mill_crpair) * maxfds);
}

static void mill_poller_add(int fd, int events) {
    struct mill_crpair *crp = &mill_crpairs[fd];
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
    int rc = epoll_ctl(mill_efd, exists ? EPOLL_CTL_MOD : EPOLL_CTL_ADD,
        fd, &ev);
    if(rc != 0) {
        /* TODO: Rollback the changes here. */
        mill_assert(0);
    }
}

static void mill_poller_rm(int fd, int events) {
    struct mill_crpair *crp = &mill_crpairs[fd];
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
    int rc = epoll_ctl(mill_efd, ev.events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL,
        fd, &ev);
    if(rc != 0) {
        /* TODO: What to do here? */
        mill_assert(0);
    }
}

static int mill_poller_wait(int timeout) {
    /* Wait for events. */
    int numevs;
    struct epoll_event evs[MILL_EPOLLSETSIZE];
    while(1) {
        numevs = epoll_wait(mill_efd, evs, MILL_EPOLLSETSIZE, timeout);
        if(numevs < 0 && errno == EINTR)
            continue;
        mill_assert(numevs >= 0);
        break;
    }
    /* Fire file descriptor events. */
    int i;
    for(i = 0; i != numevs; ++i) {
        struct mill_crpair *crp = &mill_crpairs[evs[i].data.fd];
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
        int rc = epoll_ctl(mill_efd, ev.events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL,
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
    /* Return 0 in case of time out. 1 if at least one coroutine was resumed. */
    return numevs > 0 ? 1 : 0;
}

