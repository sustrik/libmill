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

#include <assert.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdlib.h>

/******************************************************************************/
/*  Coroutines                                                                */
/******************************************************************************/

#define STACK_SIZE 16384

struct cr {
    struct cr *next;
    jmp_buf ctx;
};

/* Fake coroutine corresponding to the main thread of execution. */
struct cr main_cr = {NULL};

/* The queue of coroutines ready to run. The first one is currently running. */
struct cr *first_cr = &main_cr;
struct cr *last_cr = &main_cr;

/* Removes current coroutine from the queue and returns it to the calller. */
static struct cr *suspend() {
    struct cr *cr = first_cr;
    first_cr = first_cr->next;
    if(!first_cr)
        last_cr = NULL;
    cr->next = NULL;
    return cr;
}

/* Schedules preiously suspended coroutine for execution. */
static void resume(struct cr *cr) {
    cr->next = NULL;
    if(last_cr)
        last_cr->next = cr;
    else
        first_cr = cr;
    last_cr = cr;
}

/* Switch to a different coroutine. */
static void ctxswitch(void) {
    longjmp(first_cr->ctx, 1);
}

/* The intial part of go(). Allocates the stack for the new coroutine. */
void *mill_go_prologue() {
    if(setjmp(first_cr->ctx))
        return NULL;
    char *ptr = malloc(STACK_SIZE);
    assert(ptr);
    struct cr *cr = (struct cr*)(ptr + STACK_SIZE - sizeof (struct cr));
    cr->next = first_cr;
    first_cr = cr;
    return (void*)cr;
}

/* The final part of go(). Cleans up when the coroutine is finished. */
void mill_go_epilogue(void) {
    struct cr *cr = first_cr;
    first_cr = first_cr->next;
    char *ptr = ((char*)(cr + 1)) - STACK_SIZE;
    free(ptr);
    ctxswitch();    
}

/* Move the current coroutine to the end of the queue.
   Pass control to the new head of the queue. */
void yield(void) {
    if(first_cr == last_cr)
        return;
    if(setjmp(first_cr->ctx))
        return;
    resume(suspend());
    ctxswitch();
}

/******************************************************************************/
/*  Channels                                                                  */
/******************************************************************************/

struct chan {
    struct cr *sender;
    struct cr *receiver;
    void **src;
    void **dest;
};

chan chmake(void) {
    struct chan *ch = (struct chan*)malloc(sizeof(struct chan));
    assert(ch);
    ch->sender = NULL;
    ch->receiver = NULL;
    ch->src = NULL;
    ch->dest = NULL;
    return ch;
}

void chs(chan ch, void *val) {
    /* Only one coroutine can send at a time. */
    assert(!ch->sender);

    /* If there's a receiver already waiting, we can just unblock it. */
    if(ch->receiver) {
        *(ch->dest) = val;
        resume(ch->receiver);
        ch->receiver = NULL;
        ch->dest = NULL;
        return;
    }

    /* Otherwise we are going to yield till the receiver arrives. */
    if(setjmp(first_cr->ctx))
        return;
    ch->sender = suspend();
    ch->src = &val;

    /* Pass control to a different coroutine. */
    ctxswitch();
}

void *chr(chan ch) {
    /* Only one coroutine can receive from a channel at a time. */
    assert(!ch->receiver);

    /* If there's a sender already waiting, we can just unblock it. */
    if(ch->sender) {
        void *val = *(ch->src);
        resume(ch->sender);
        ch->sender = NULL;
        ch->src = NULL;
        return val;
    }

    /* Otherwise we are going to yield till the sender arrives. */
    void *val;
    if(setjmp(first_cr->ctx))
        return val;
    ch->receiver = suspend();
    ch->dest = &val;

    /* Pass control to a different coroutine. */
    ctxswitch();
}	

void chclose(chan ch) {
    // TODO
    free(ch);
}

