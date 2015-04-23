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

#include "mill.h"
#include "util.c"
#include <poll.h>

/* Linked list of all paused coroutines. The list is ordered.
   First coroutine to be resumed comes first et c. */
static struct cr_ *paused_ = NULL;

/* Pollset used for waiting for file descriptors. */
static int wait_size_ = 0;
static int wait_capacity_ = 0;
static struct pollfd *wait_fds_ = NULL;
static struct cr_ **wait_crs_ = NULL;

/* Removes current coroutine from the queue and returns it to the calller. */
static struct cr_ *suspend() {
    struct cr_ *cr = first_cr_;
    first_cr_ = first_cr_->next;
    if(!first_cr_)
        last_cr_ = NULL;
    cr->next = NULL;
    return cr;
}

/* Schedules preiously suspended coroutine for execution. */
static void resume(struct cr_ *cr) {
    cr->next = NULL;
    if(last_cr_)
        last_cr_->next = cr;
    else
        first_cr_ = cr;
    last_cr_ = cr;
}

/* Pass control to a different coroutine.
   If necessary, wait for a timeout or an external event. */
static void ctxswitch(void) {
    /* Resume coroutine from the front of the queue if possible. */
    if(first_cr_)
        longjmp(first_cr_->ctx, 1);

    /* The execution would block. Let's panic. */
    assert(paused_ || wait_size_);

    /* Compute the time till next expired pause. */
    int timeout = -1;
    if(paused_) {
        uint64_t nw = now();
        timeout = nw >= paused_->expiry ? 0 : paused_->expiry - nw;
    }

    /* Wait for events. */
    int rc = poll(wait_fds_, wait_size_, timeout);
    assert(rc >= 0);

    /* Resume all paused coroutines that have expired. */
    if(paused_) {
        uint64_t nw = now();
		while(paused_ && paused_->expiry <= nw) {
            struct cr_ *cr = paused_;
            paused_ = cr->next;
            resume(cr);
		}
    }

    /* Resume coroutines waiting for file descriptors as appropriate. */
    int i;
    for(i = 0; i != wait_size_ && rc; ++i) {
        if(wait_fds_[i].revents) {
            resume(wait_crs_[i]);
            wait_fds_[i] = wait_fds_[wait_size_ - 1];
            wait_crs_[i] = wait_crs_[wait_size_ - 1];
            --wait_size_;
            --rc;
        }
    }

	/* Pass control to the resumed coroutine. */
	longjmp(first_cr_->ctx, 1);
}

void yield(void) {
    /* Optimisation if there's only one coroutine in the queue. */
    if(first_cr_ == last_cr_)
        return;

    /* Store the current state. */
    if(setjmp(first_cr_->ctx))
        return;

    /* Move the current coroutine to the end of the queue. */
    resume(suspend());

    /* Pass control to another coroutine. */
    ctxswitch();
}

/* Pause current coroutine for a specified time interval. */
static void pause(unsigned long ms) {
    /* No point in waiting. However, let's give other coroutines a chance. */
    if(ms <= 0) {
        yield();
        return;
    }

    /* Store the current state. */
    if(setjmp(first_cr_->ctx))
        return;
    
    /* Move the coroutine into the right place in the ordered list
       of paused coroutines. */
    struct cr_ *cr = suspend();
    cr->expiry = now() + ms;
    struct cr_ **it = &paused_;
    while(*it && (*it)->expiry <= cr->expiry)
        it = &((*it)->next);
    cr->next = *it;
    *it = cr;

    /* Pass control to a different coroutine. */
    ctxswitch();
}

/* Wait for events from a file descriptor. */
static void wait(int fd, short events) {
    /* Store the current state. */
    if(setjmp(first_cr_->ctx))
        return;

    /* Grow the pollset as needed. */
    if(wait_size_ == wait_capacity_) {
        wait_capacity_ = wait_capacity_ ? wait_capacity_ * 2 : 64;
        wait_fds_ = realloc(wait_fds_,
            wait_capacity_ * sizeof(struct pollfd));
        wait_crs_ = realloc(wait_crs_, wait_capacity_ * sizeof(struct cr_*));
    }

    /* Remove the current coroutine from the queue. */
    struct cr_ *cr = suspend();

    /* Add the new file descriptor to the pollset. */
    wait_fds_[wait_size_].fd = fd;
    wait_fds_[wait_size_].events = events;
    wait_fds_[wait_size_].revents = 0;
    wait_crs_[wait_size_] = cr;
    ++wait_size_;

    /* Pass control to a different coroutine. */
    ctxswitch();
}

#include "chan.c"
#include "lib.c"
#include "dbg.c"

