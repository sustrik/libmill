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

typedef void* event;

struct mill_base;

typedef void (*mill_handler_fn) (struct mill_base *self, event ev);

struct mill_base {
    mill_handler_fn handler; 
    int state;
    struct mill_base *parent;
};

void mill_base_init (
    struct mill_base *self,
    mill_handler_fn handler,
    struct mill_base *parent);

#define mill_getevent(statearg, eventarg)\
    self->mill_base.state = (statearg);\
    return;\
    state##statearg:\
    (eventarg) = ev;

#define mill_return(resarg)\
    self->mill_result = (resarg);\
    return;

#define mill_result(coro)\
    ((coro)->mill_result)

struct mill_coroutine_wait {
    struct mill_base mill_base;
    uv_timer_t timer;
};

void mill_call_wait (
    struct mill_coroutine_wait *self,
    int milliseconds,
    struct mill_base *parent);

#endif

