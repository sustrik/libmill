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

/* These flags are used to construct 'flags' member in mill_cfh. */
#define MILL_FLAG_DEALLOCATE 1
#define MILL_FLAG_CANCELED 2

#define mill_cont(ptr, type, member) \
    (ptr ? ((type*) (((char*) ptr) - offsetof(type, member))) : NULL)

struct mill_cfh;
struct mill_loop;

typedef void (*mill_handler_fn) (
    struct mill_cfh *self,
    struct mill_cfh* ev);

struct mill_cfh {
    mill_handler_fn handler; 
    int state;
    int flags;
    int tag;
    int err;
    struct mill_cfh *parent;
    struct mill_cfh *children;
    struct mill_cfh *next;
    struct mill_cfh *prev;
    struct mill_loop *loop;
};

void mill_cfh_init (
    struct mill_cfh *self,
    mill_handler_fn handler,
    struct mill_cfh *parent,
    struct mill_loop *loop,
    int flags,
    int tag);

void mill_cfh_term (struct mill_cfh *self);

void mill_cfh_emit (struct mill_cfh *self);

void mill_cfh_cancelall (struct mill_cfh *self);

int mill_cfh_haschildren (struct mill_cfh *self);

void mill_cancel (void *cf);

#define mill_wait(statearg)\
    do {\
        cf->mill_cfh.state = (statearg);\
        return;\
        state##statearg:\
        ;\
    } while (0)

#define mill_return(errarg)\
    do {\
        cf->mill_cfh.err = (errarg);\
        goto mill_finally;\
    } while (0)

#define mill_cancelall(statearg)\
    do {\
        mill_cfh_cancelall (&cf->mill_cfh);\
        while (1) {\
            if (!mill_cfh_haschildren (&cf->mill_cfh))\
                break;\
            cf->mill_cfh.state = (statearg);\
            return;\
            state##statearg:\
            ;\
        }\
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
    struct mill_cfh *first;
    struct mill_cfh *last;
};

void mill_loop_init (struct mill_loop *self);
void mill_loop_term (struct mill_loop *self);
void mill_loop_run (struct mill_loop *self);
void mill_loop_emit (struct mill_loop *self, struct mill_cfh *base);

#endif

