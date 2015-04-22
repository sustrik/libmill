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
#include <unistd.h>
#include <sys/time.h>

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

#include "chan.c"
#include "lib.c"
#include "dbg.c"

