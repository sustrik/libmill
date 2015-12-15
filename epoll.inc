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
#include <stdint.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/resource.h>

#include "cr.h"
#include "utils.h"

#define MILL_ENDLIST 0xffffffff

#define MILL_EPOLLSETSIZE 128

/* Global pollset. */
static int mill_efd = -1;

/* Epoll allows to register only a single pointer with a file decriptor.
   However, we may need two pointers to coroutines. One for the coroutine
   waiting to receive data from the descriptor, one for the coroutine waiting
   to send data to the descriptor. Thus, we are going to keep an array of
   pointer pairs for each file descriptor. */
struct mill_crpair {
    struct mill_cr *in;
    struct mill_cr *out;
    uint32_t currevs;
    /* 1-based index, 0 stands for "not part of the list", MILL_ENDLIST
       stads for "no more elements in the list. */
    uint32_t next;
};

static struct mill_crpair *mill_crpairs = NULL;
static int mill_ncrpairs = 0;
static uint32_t mill_changelist = MILL_ENDLIST;

void mill_poller_init(void) {
    struct rlimit rlim;
    int rc = getrlimit(RLIMIT_NOFILE, &rlim);
    if(mill_slow(rc < 0)) return;
    mill_ncrpairs = rlim.rlim_max;
    mill_crpairs = (struct mill_crpair*)
        calloc(mill_ncrpairs, sizeof(struct mill_crpair));
    if(mill_slow(!mill_crpairs)) {errno = ENOMEM; return;}
    mill_efd = epoll_create(1);
    if(mill_slow(mill_efd < 0)) {
        free(mill_crpairs);
        mill_crpairs = NULL;
        return;
    }
    errno = 0;
}

pid_t mill_fork(void) {
    pid_t pid = fork();
    if(pid != 0)
        return pid;
    /* In the child process we have to create a new kernel-side pollset,
       identical to the one in the parent process, yet not sharing its
       destiny. */
    if(mill_efd != -1) {
        int rc = close(mill_efd);
        mill_assert(rc == 0);
        mill_efd = epoll_create(1);
        mill_assert(mill_efd >= 0);
        int fd;
        for(fd = 0; fd != mill_ncrpairs; ++fd) {
            if(mill_crpairs[fd].currevs) {
                struct epoll_event ev;
                ev.data.fd = fd;
                ev.events = 0;
                if(mill_crpairs[fd].currevs & FDW_IN)
                    ev.events |= EPOLLIN;
                if(mill_crpairs[fd].currevs & FDW_OUT)
                    ev.events |= EPOLLOUT;
                rc = epoll_ctl(mill_efd, EPOLL_CTL_ADD, fd, &ev);
                mill_assert(rc == 0);
            }
        }
    }
    errno = 0;
    return 0;
}

static void mill_poller_add(int fd, int events) {
    struct mill_crpair *crp = &mill_crpairs[fd];
    if(events & FDW_IN) {
        if(crp->in)
            mill_panic(
                "multiple coroutines waiting for a single file descriptor");
        crp->in = mill_running;
    }
    if(events & FDW_OUT) {
        if(crp->out)
            mill_panic(
                "multiple coroutines waiting for a single file descriptor");
        crp->out = mill_running;
    }
    if(!crp->next) {
        crp->next = mill_changelist;
        mill_changelist = fd + 1;
    }
}

static void mill_poller_rm(int fd, int events) {
    struct mill_crpair *crp = &mill_crpairs[fd];
    if(events & FDW_IN)
        crp->in = NULL;
    if(events & FDW_OUT)
        crp->out = NULL;
    if(!crp->next) {
        crp->next = mill_changelist;
        mill_changelist = fd + 1;
    }
}

static void mill_poller_clean(int fd) {
    struct mill_crpair *crp = &mill_crpairs[fd];
    mill_assert(!crp->in);
    mill_assert(!crp->out);
    /* Remove the file descriptor from the pollset, if it is still present. */
    if(crp->currevs) {   
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = 0;
        int rc = epoll_ctl(mill_efd, EPOLL_CTL_DEL, fd, &ev);
        mill_assert(rc == 0 || errno == ENOENT);
    }
    /* Clean the cache. */
    crp->currevs = 0;
    if(!crp->next) {
        crp->next = mill_changelist;
        mill_changelist = fd + 1;
    }
}

static int mill_poller_wait(int timeout) {
    /* Apply any changes to the pollset.
       TODO: Use epoll_ctl_batch once available. */
    while(mill_changelist != MILL_ENDLIST) {
        int fd = mill_changelist - 1;
        struct mill_crpair *crp = &mill_crpairs[fd];
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = 0;
        if(crp->in)
            ev.events |= EPOLLIN;
        if(crp->out)
            ev.events |= EPOLLOUT;
        if(crp->currevs != ev.events) {
            int op;
            if(!ev.events)
                 op = EPOLL_CTL_DEL;
            else if(!crp->currevs)
                 op = EPOLL_CTL_ADD;
            else
                 op = EPOLL_CTL_MOD;
            crp->currevs = ev.events;
            int rc = epoll_ctl(mill_efd, op, fd, &ev);
            mill_assert(rc == 0);
        }
        mill_changelist = crp->next;
        crp->next = 0;
    }
    /* Wait for events. */
    struct epoll_event evs[MILL_EPOLLSETSIZE];
    int numevs;
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
        int inevents = 0;
        int outevents = 0;
        /* Set the result values. */
        if(evs[i].events & EPOLLIN)
            inevents |= FDW_IN;
        if(evs[i].events & EPOLLOUT)
            outevents |= FDW_OUT;
        if(evs[i].events & (EPOLLERR | EPOLLHUP)) {
            inevents |= FDW_ERR;
            outevents |= FDW_ERR;
        }
        /* Resume the blocked coroutines. */  
        if(crp->in == crp->out) {
            mill_resume(crp->in, inevents | outevents);
            mill_poller_rm(evs[i].data.fd, FDW_IN | FDW_OUT);
        }
        else {
            if(crp->in && inevents) {
                mill_resume(crp->in, inevents);
                mill_poller_rm(evs[i].data.fd, FDW_IN);
            }
            if(crp->out && outevents) {
                mill_resume(crp->out, outevents);
                mill_poller_rm(evs[i].data.fd, FDW_OUT);
            }
        }
    }
    /* Return 0 in case of time out. 1 if at least one coroutine was resumed. */
    return numevs > 0 ? 1 : 0;
}

