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

#include <stdlib.h>
#include <setjmp.h>
#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

static void dotrace(const char *text);

/******************************************************************************/
/* Utilities                                                                  */
/******************************************************************************/

static uint64_t now() {
    struct timeval tv;
    int rc = gettimeofday(&tv, NULL);
    assert(rc == 0);
    return ((uint64_t)tv.tv_sec) * 1000 + (((uint64_t)tv.tv_usec) / 1000);
}

/******************************************************************************/
/* Stack management                                                           */
/******************************************************************************/

#define STACK_SIZE 16384

/* ID to use for the next created coroutine. */
static unsigned long nextid = 0;

/* Individual coroutine. Actual stack precedes this structure to keep page
   misses to minimum. */
/* TODO: On architectures where the stack grows upwards, it should follow this
   structure. */
struct cr {
    struct cr *next;
    jmp_buf ctx;
    unsigned long id;
    const char *desc;
    uint64_t expiry;
};

static struct cr *alloc_stack(const char *desc) {
    char *ptr = malloc(STACK_SIZE);
    assert(ptr);
    struct cr *cr = (struct cr*)(ptr + STACK_SIZE - sizeof (struct cr));
    cr->id = nextid;
    nextid++;
    cr->desc = desc;
    return cr;
}

static void free_stack(struct cr *stack) {
    char *ptr = ((char*)(stack + 1)) - STACK_SIZE;
    free(ptr);
}

/******************************************************************************/
/* Coroutine ring                                                             */
/******************************************************************************/

/* A fake stack for the main coroutine. */
struct cr mainstack;

/* Ring of all active coroutines. */
struct cr *ring = NULL;

void ring_init(void) {
    mainstack.next = &mainstack;
    mainstack.id = 0;
    mainstack.desc = "main()";
    ring = &mainstack;
}

struct cr *ring_current(void) {
    return ring ? ring->next : NULL;
}

void ring_add(struct cr *cr) {
    if(!ring) {
        cr->next = cr;
        ring = cr;
        return;
    }
    assert(ring->next->next);
    ring = ring->next;
    cr->next = ring->next;
    assert(cr);
    ring->next = cr;
}

void ring_push(struct cr *cr) {
    ring_add(cr);
    ring = ring->next;
}

struct cr *ring_rm(void) {
    struct cr *cr;

    assert(ring);
    cr = ring->next;
    assert(cr->next);
    if(cr == ring)
        ring = NULL;
    else
        ring->next = cr->next;
    return cr;
}

void ring_next(void) {
    ring = ring->next;
}

/******************************************************************************/
/* Actual coroutine implementation                                            */
/******************************************************************************/

/* List of all sleeping coroutines. */
struct cr *timers = NULL;

struct cr *idlecr = NULL;

static void schedule(void) {
    if (ring_current())
        longjmp(ring_current()->ctx, 1);
    longjmp(idlecr->ctx, 1); 
}

static void idle() {
    /* If there are no timers, the program is stuck. */
    assert(timers);

    uint64_t nw = now();

    if(timers->expiry > nw)
        usleep((timers->expiry - nw) * 1000);
    struct cr *next = timers->next;
    ring_add(timers);
    timers = next;
    dotrace("resuming");
    schedule();
}

static void init() {
    /* Very first time mill is called. We have to make a fake stack for the
       main coroutine. */
    ring_init();

    dotrace("starting");

    /* And create an idle coroutine. */
    idlecr = alloc_stack("_idle");
    idlecr->next = NULL;
    if(setjmp(idlecr->ctx))
        idle();
}

void *go_prologue_(const char *desc) {
    struct cr *stack;

    if (!ring)
        init();

    /* We are going to schedule the new coroutine immediately, so that we can
       get rid of it straight away if it's happens not to block. */
    if(setjmp(ring_current()->ctx)) {
        /* Parent corouting was re-scheduled. Proceed as normal. */
        return NULL;
    }

    /* Allocate a stack for the new coroutine. */
    stack = alloc_stack(desc);
    ring_add(stack);

    dotrace("starting");

    return (void*)stack;
}

void go_epilogue_(void) {
    struct cr *stack;

    dotrace("terminating");

    /* Coroutine is done. Remove it from the ring and free its stack. */
    stack = ring_rm();
    free_stack(stack);

    /* Switch to the next coroutine. */
    schedule();
}

void yield(void) {
    struct cr *stack;

    if (!ring)
        init();

    dotrace("yielding");

    /* Store the state of the currently executing coroutine. */
    if (setjmp(ring_current()->ctx))
        return;
	
    /* Move to the next coroutine. */
    ring_next();
    dotrace("resuming");
    schedule();
}

/******************************************************************************/
/* Channels                                                                   */
/******************************************************************************/

struct chan {
    struct cr *sender;
    struct cr *receiver;
    chan_val val;
    chan_val *dest;
    int idx;
    int *idxdest;
};

typedef struct chan* chan;

chan chan_init(void) {
    struct chan *ch = (struct chan*)malloc(sizeof(struct chan));
    assert(ch);
    ch->sender = NULL;
    ch->receiver = NULL;
    ch->val.ptr = NULL;
    ch->dest = NULL;
    ch->idx = -1;
    ch->idxdest = NULL;
    return ch;
}

void chan_send(chan ch, chan_val val) {
    assert(!ch->sender);

    /* If there's a receiver already waiting, we can just unblock it. */
    if(ch->receiver) {
        if(ch->dest)
            *(ch->dest) = val;
        if(ch->idxdest)
            *(ch->idxdest) = ch->idx;
        ring_push(ch->receiver);
        ch->receiver = NULL;
        ch->dest = NULL;
        return;
    }

    /* Otherwise we are going to yield till the receiver arrives. */
    ch->sender = ring_current();
    ch->val = val;
    if(setjmp(ring_current()->ctx)) {
        dotrace("resuming");
        return;
    }
    dotrace("blocks on chan_send");
    ring_rm();
    schedule();
}

chan_val chan_recv(chan ch) {
    chan_val val;

    assert(!ch->receiver);

    /* If there's a sender already waiting, we can just unblock it. */
    if(ch->sender) {
        val = ch->val;
        ring_push(ch->sender);
        ch->sender = NULL;
        ch->val.ptr = NULL;
        return val;
    }

    /* Otherwise we are going to yield till the sender arrives. */
    ch->receiver = ring_current();
    ch->dest = &val;
    if(setjmp(ring_current()->ctx)) {
        dotrace("resuming");
        return val;
    }
    dotrace("blocks on chan_recv");
    ring_rm();
    schedule();
}	

void chan_close(chan ch) {
    // TODO
    free(ch);
}

int chan_tryselectv(chan_val *val, chan *chans, int nchans) {
    /* TODO: For fairness sake we should choose a random number up to 'count'
       and start the array traversal at that index. */
    int i;
    for(i = 0; i != nchans; ++i) {
        chan ch = chans[i];
        assert(!ch->receiver);
		if(ch->sender) {
            if(val)
		        *val = ch->val;
		    ring_push(ch->sender);
		    ch->sender = NULL;
		    ch->val.ptr = NULL;
		    return i;
		}
    }
    return -1;
}

int chan_selectv(chan_val *val, chan *chans, int nchans) {

    /* Try to select a channel in a non-blocking way. */
    int res = chan_tryselectv(val, chans, nchans);
    if(res >= 0)
        return res;

    /* None of the channels is ready. Mark them as such. */
    int i;
    for(i = 0; i != nchans; ++i) {
        chan ch = chans[i];
	    ch->receiver = ring_current();
		ch->dest = val;
        ch->idx = i;
        ch->idxdest = &res;
    }

    /* Yield till one of the channels becomes ready. */
	if(!setjmp(ring_current()->ctx)) {
		dotrace("blocks on chan_select");
		ring_rm();
		schedule();
    }

    /* One of the channels became available. */
    for(i = 0; i != nchans; ++i) {
         chan ch = chans[i];
         ch->receiver = NULL;
         ch->dest = NULL;
         ch->idx = -1;
         ch->idxdest = NULL;
    }

    /* This coroutine have got to the front of the queue unfairly.
       Let's put it back where it belongs. */
    /* TODO: We should not mess with the coroutines position in the ring
       at all .*/
    yield();

    return res;
}

int chan_select_(int block, chan_val *val, ...) {
    int count = 0;
    va_list v;
    va_start(v, val);
    while(1) {
        chan ch = va_arg(v, chan);
        if(!ch)
            break;
        ++count;
    }
    va_end(v);
    assert(count > 0);
    chan chans[count];
    int i;
    va_start(v, val);
    for(i = 0; i != count; ++i)
        chans[i] = va_arg(v, chan);
    va_end(v);
    if(block)
        return chan_selectv(val, chans, count);
    return chan_tryselectv(val, chans, count);
}

/******************************************************************************/
/* Coroutine library                                                          */
/******************************************************************************/

void musleep(unsigned int microseconds) {
    if (!ring)
        init();
    if(microseconds <= 0)
        return;
    uint64_t ms = microseconds / 1000;
    if(microseconds % 1000 != 0)
        ++ms;
    dotrace("goes to sleep");
    struct cr *current = ring_rm();
    current->expiry = now() + ms;

    struct cr **it = &timers;
    while(*it && (*it)->expiry <= current->expiry)
        it = &((*it)->next);
    current->next = *it;
    *it = current;
    if(setjmp(current->ctx))
        return;
    schedule();    
}

void msleep(unsigned int seconds) {
    musleep(seconds * 1000000);
}

static void after_(int ms, chan ch) {
    musleep(ms * 1000);
    chan_val val;
    val.ptr = NULL;
    chan_send(ch, val);
    chan_close(ch);
}

chan after(int ms) {
    chan ch = chan_init();
    go(after_(ms, ch));
    return ch;
}

/******************************************************************************/
/* Debugging support                                                          */
/******************************************************************************/

static int tracing = 0;

void trace(void) {
    tracing = 1;
}

static void cr_function(struct cr *stack, char *buf, size_t len) { 
    if(!len)
        return;
    const char *it = stack->desc;
    while(len > 1 && (isalnum(*it) || *it == '_')) {
        *buf = *it;
        ++it,++buf,--len;
    }
    *buf = 0;
}

static void dotrace(const char *text) {
    if (!tracing)
        return;
    char func[64];
    cr_function(ring_current(), func, sizeof(func));
    fprintf(stderr, "===> %lu: %s - %s\n", ring_current()->id, func, text);
}

static void goredump_dummy(void) {
}

goredump_fn_ goredump_(const char *file, int line) {
    struct cr *stack;
    char func[256];

    fprintf(stderr, "+--------------------------------------------------------"
        "-----------------------\n");
    fprintf(stderr, "| GOREDUMP (%s:%d)\n", file, line); // TODO: add time
    stack = ring_current();
    if(stack) {
        cr_function(stack, func, sizeof(func));
        fprintf(stderr, "|    %lu: %s - running\n", stack->id, func);
        stack = stack->next;
        if(stack != ring_current()) {
		    while(1) {
                cr_function(stack, func, sizeof(func));
		        fprintf(stderr, "|    %lu: %s - ready\n", stack->id, func);
		        stack = stack->next;
		        if(stack == ring_current())
		            break;
		    }
        }
    }
    if(timers) {
        uint64_t nw = now();
        stack = timers;
        while(stack) {
            unsigned long rmnd = nw > stack->expiry ?
                0 : stack->expiry - nw;
            cr_function(stack, func, sizeof(func));
            fprintf(stderr, "|    %lu: %s - sleeping (%lu ms remaining)\n",
                stack->id, func, rmnd);
            stack = stack->next;
        }
    }
    fprintf(stderr, "+--------------------------------------------------------"
        "-----------------------\n");
    return goredump_dummy;
}

