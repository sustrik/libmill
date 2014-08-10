/*
    Copyright (c) 2014 Martin Sustrik  All rights reserved.

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

/* This header is used inernally by mill. Don't use it directly in your code! */

#ifndef mill_h_included
#define mill_h_included

#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <uv.h>

/******************************************************************************/
/* Tracing support.                                                           */
/******************************************************************************/

void _mill_trace ();
void mill_trace_select (void *cfptr);

/******************************************************************************/
/*  Coroutine metadata.                                                       */
/******************************************************************************/

/* Constant tag is used to make sure that the coframe is valid. If its type
   member doesn't point to something that begins with mill_type_tag, the
   coframe is considered invalid. */
#define mill_type_tag 0x4efd36df

/* Main body of the coroutine. Handles all incoming events.
   'cfptr' points to the coframe of the coroutine being evaluated.
   'event' either points to the coframe of the child coroutine that have just
   terminated or 0 in case the parent have asked the coroutine to cancel.
   Returns 0 if the event was successfully processed, -1 otherwise. */
typedef int (*mill_fn_handler) (void *cfptr, void *event);

/* Final step of the coroutine.
   Copies all output arguments to their intended destinations. */
typedef void (*mill_fn_output) (void *cfptr);

/* Structure containing all the coroutine metadata. */
struct mill_type {
    int tag;
    mill_fn_handler handler;
    mill_fn_output output;
    const char *name;
};

/******************************************************************************/
/* Coframe head is common to all coroutines.                                  */
/******************************************************************************/

/* This structure is placed at the beginning of each coframe. */
struct mill_cfh {

    /* Coroutine metadata. */
    const struct mill_type *type;

    /* Coroutine's "program counter". Specifies which point in the coroutine
       is currently being executed. The values are specific to individual
       coroutines. */
    int pc;

    /* Once the coroutine finishes, the coframe is put into the loop's event
       queue and later on, if the event can't be processed immediately, into
       the parent coroutine's pending queue. This member implements the
       single-linked list that forms the queue. */
    struct mill_cfh *nextev;

    /* The queue of pending events that can't be processed at the moment. */
    struct mill_cfh *pfirst;
    struct mill_cfh *plast;

    /* Parent corouine. */
    struct mill_cfh *parent;

    /* List of child coroutines. */
    struct mill_cfh *children;

    /* Double-linked list of sibling coroutines. */
    struct mill_cfh *next;
    struct mill_cfh *prev;

    /* Event loop that executes this coroutine. */
    struct mill_loop *loop;
};

/******************************************************************************/
/* The event loop.                                                            */
/******************************************************************************/

struct mill_loop
{
    /* Underlying libuv loop. */
    uv_loop_t uv_loop;

    /* Libuv hook that processes the mill events. */
    uv_idle_t idle;

    /* Local event queue. Items in the list are processed using
       the hook above. */
    struct mill_cfh *first;
    struct mill_cfh *last;
};

/* Initialise the loop object. */
void mill_loop_init (struct mill_loop *self);

/* Deallocate resources used by the loop. This function can be called only
   after mill_loop_run exits. */
void mill_loop_term (struct mill_loop *self);

/* Start the loop. The loop runs until the top-level coroutine exits. */
void mill_loop_run (struct mill_loop *self);

/* Enqueue the event to the loop's event queue. */
void mill_loop_emit (struct mill_loop *self, struct mill_cfh *ev);

/******************************************************************************/
/*  Helpers used to implement mill keywords.                                  */
/******************************************************************************/

#define mill_select(pcarg)\
    {\
        cf->mill_cfh.pc = (pcarg);\
        return 0;\
        mill_pc_##pcarg:\
        if (0) {

#define mill_case(pcarg, typearg)\
            goto mill_pc2_##pcarg;\
        }\
        else if (event &&\
              ((struct mill_cfh*) event)->type == &mill_type_##typearg) {\
            mill_trace_select (event);

#define mill_cancel(pcarg)\
            goto mill_pc2_##pcarg;\
        }\
        else if (event == 0) {

#define mill_endselect(pcarg)\
            goto mill_pc2_##pcarg;\
        }\
        else if (event == 0) {\
            goto mill_finally;\
        }\
        else {\
            return -1;\
        }\
        mill_pc2_##pcarg:\
        ;\
    }

#define mill_syswait(pcarg, ptrarg)\
    do {\
        cf->mill_cfh.pc = (pcarg);\
        return;\
        mill_pc_##pcarg:\
        mill_putptr (event, (ptrarg));\
    } while (0)

void mill_coframe_init (
    void *cfptr,
    const struct mill_type *type,
    void *parent,
    struct mill_loop *loop);
void mill_emit (
    void *cfptr);
void mill_add_child (
    void *cfptr,
    void *child);
void mill_putptr (
    void *ptr,
    void **dst);
void mill_cancel_children (
    void *cfptr);
int mill_has_children (
    void *cfptr);

#endif

