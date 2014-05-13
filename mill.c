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

#include "mill.h"

#include <stdio.h>

/******************************************************************************/
/* Generic stuff.                                                             */
/******************************************************************************/

#define cont(ptr, type, member) \
    (ptr ? ((type*) (((char*) ptr) - offsetof(type, member))) : NULL)

void ____base_init (
    struct ____base *self,
    ____handler_fn handler,
    struct ____base *parent)
{
    self->handler = handler;
    self->state = 0;
    self->parent = parent;
}

void ____base_term (struct ____base *self)
{
}

/******************************************************************************/
/* Wait coroutine.                                                            */
/******************************************************************************/

static void wait_handler(struct ____base *base, event ev)
{
    struct ____coroutine_wait *self = (struct ____coroutine_wait*) base;
    ____base_term (&self->____base);
}

static void wait_cb (uv_timer_t *timer)
{
    wait_handler (&cont (timer, struct ____coroutine_wait, timer)->____base, 0);
}

void ____call_wait (
    struct ____coroutine_wait *self,
    int milliseconds,
    struct ____base *parent)
{
    ____base_init (&self->____base, wait_handler, parent);
    uv_timer_init(uv_default_loop(), &self->timer);
    uv_timer_start(&self->timer, wait_cb, milliseconds, 0);
}

