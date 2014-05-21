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

#ifndef MILL_H_INCLUDED
#define MILL_H_INCLUDED

#include <assert.h>
#include <stdlib.h>
#include <uv.h>

/******************************************************************************/
/* Generic stuff.                                                             */
/******************************************************************************/

/* These flags are used to construct 'flags' member in mill_coframe_head. */
#define MILL_FLAG_DEALLOCATE 1
#define MILL_FLAG_CANCELED 2

#define mill_cont(ptr, type, member) \
    (ptr ? ((type*) (((char*) ptr) - offsetof(type, member))) : NULL)

struct mill_coframe_head;
struct mill_loop;

typedef void (*mill_handler_fn) (
    struct mill_coframe_head *self,
    struct mill_coframe_head* ev);

struct mill_coframe_head {
    mill_handler_fn handler; 
    int state;
    int flags;
    int tag;
    int err;
    struct mill_coframe_head *parent;
    struct mill_coframe_head *next;
    struct mill_loop *loop;
};

void mill_coframe_head_init (
    struct mill_coframe_head *self,
    mill_handler_fn handler,
    struct mill_coframe_head *parent,
    struct mill_loop *loop,
    int flags,
    int tag);

void mill_coframe_head_term (struct mill_coframe_head *self);

void mill_coframe_head_emit (struct mill_coframe_head *self, int err);

#define mill_wait(statearg)\
    do {\
        cf->mill_cfh.state = (statearg);\
        return;\
        state##statearg:\
        ;\
    } while (0)

#define mill_return(errarg)\
    do {\
        cf->mill_cfh.err = (arrarg);\
        mill_coframe_head_emit (&cf->mill_cfh);\
        return;\
    } while (0)

/******************************************************************************/
/* The event loop.                                                            */
/******************************************************************************/

struct mill_loop
{
    uv_loop_t uv_loop;
    uv_idle_t idle;

    /* Local event queue. Items in this list are processed immediately,
       before control is returned to libuv. */
    struct mill_coframe_head *first;
    struct mill_coframe_head *last;
};

void mill_loop_init (struct mill_loop *self);
void mill_loop_term (struct mill_loop *self);
void mill_loop_run (struct mill_loop *self);
void mill_loop_emit (struct mill_loop *self, struct mill_coframe_head *base);

#endif

