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
#include <stdlib.h>

#define mill_cont(ptr, type, member) \
    (ptr ? ((type*) (((char*) ptr) - offsetof(type, member))) : NULL)

#define uv_assert(x)\
    do {\
        if (x < 0) {\
            fprintf (stderr, "%s: %s (%s:%d)\n", uv_err_name (x),\
                uv_strerror (x), __FILE__, __LINE__);\
            fflush (stderr);\
            abort ();\
        }\
    } while (0)

/******************************************************************************/
/* The event loop. */
/******************************************************************************/

static void loop_cb (uv_idle_t* handle)
{
    struct mill_loop *self;
    struct mill_cfh *src;

    self = mill_cont (handle, struct mill_loop, idle);

    while (self->first) {
        src = self->first;

        if (!src->parent) {
            uv_stop (&self->uv_loop);
            return;
        }

        /* Remove the child from parent's list of children. */
        if (src != 0 && src != (struct mill_cfh*) -1) {
            if (src->parent->children == src)
                src->parent->children = src->next;
            if (src->prev)
                src->prev->next = src->next;
            if (src->next)
                src->next->prev = src->prev;
        }

        src->parent->type->handler (src->parent, src);
        self->first = src->next;

        /* Deallocate auto-allocated coframes. */
        if (src->flags & mill_flag_deallocate)
            free (src);
    }
    self->last = 0;
}

void mill_loop_init (
    struct mill_loop *self)
{
    int rc;

    rc = uv_loop_init (&self->uv_loop);
    uv_assert (rc);
    rc = uv_idle_init (&self->uv_loop, &self->idle);
    uv_assert (rc);
    rc = uv_idle_start (&self->idle, loop_cb);
    uv_assert (rc);
    self->first = 0;
    self->last = 0;
}

void mill_loop_term (
    struct mill_loop *self)
{
    int rc;

    rc = uv_idle_stop (&self->idle);
    uv_assert (rc);
    rc = uv_loop_close (&self->uv_loop);
    uv_assert (rc);
}

void mill_loop_run (
    struct mill_loop *self)
{
    int rc;

    rc = uv_run (&self->uv_loop, UV_RUN_DEFAULT);
    uv_assert (rc);
}

void mill_loop_emit (
    struct mill_loop *self,
    struct mill_cfh *ev)
{
    if (self->first == 0)
        self->first = ev;
    else
        self->last->next = ev;
    ev->next = 0;
    self->last = ev;
}

/******************************************************************************/
/*  Mill keywords.                                                            */
/******************************************************************************/

void mill_getresult (void *cfptr, void **who, int *err)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;
    if (who)
        *who = (void*) cfh;
    if (err)
        *err = cfh->err;
}

void mill_cancel (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;
    assert (cfh->type->tag == mill_type_tag);
    cfh->type->handler (cfh, mill_event_term);
}

void *mill_typeof (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;
    assert (cfh->type && cfh->type->tag == mill_type_tag);
    return (void*) &cfh->type;
}

void mill_emit (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr; 
    mill_loop_emit (cfh->loop, cfh);
}

void mill_cancel_children (void *cfptr)
{
    struct mill_cfh *cfh;
    struct mill_cfh *child;

    cfh = (struct mill_cfh*) cfptr; 
    for (child = cfh->children; child != 0; child = child->next)
        child->type->handler (child,  mill_event_term);
}

int mill_has_children (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;
    return cfh->children ? 1 : 0;
}

/******************************************************************************/
/*  coroutine msleep                                                          */
/******************************************************************************/

static void mill_handler_msleep (void *cfptr, void *event)
{
    int rc;
    struct mill_cf_msleep *cf;

    cf = (struct mill_cf_msleep*) cfptr;

    if (event == mill_event_init)
        return;

    /* Cancel the timer. */
    if (event == mill_event_term) {
        rc = uv_timer_stop (&cf->timer);
        uv_assert (rc);
        cf->mill_cfh.err = ECANCELED;
        mill_emit (cf);
        return;
    }
}

static void msleep_cb (
    uv_timer_t *timer)
{
    struct mill_cf_msleep *cf;

    cf = mill_cont (timer, struct mill_cf_msleep, timer);

    /* The coroutine is done. */
    mill_emit (cf);
}

const struct mill_type mill_type_msleep =
    {mill_type_tag, mill_handler_msleep};

void *mill_call_msleep (
    void *cfptr,
    const struct mill_type *type,
    struct mill_loop *loop,
    void *parent,
    int milliseconds)
{
    mill_callimpl_prologue (msleep);

    /* Launch the timer. */
    uv_timer_init (&loop->uv_loop, &cf->timer);
    uv_timer_start(&cf->timer, msleep_cb, milliseconds, 0);

    mill_callimpl_epilogue (msleep);
}

int msleep (int milliseconds)
{
    assert (0);
}

