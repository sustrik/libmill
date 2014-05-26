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
/* Coframe head -- common to all coroutines.                                  */
/******************************************************************************/

void mill_cfh_init (
    struct mill_cfh *self,
    const struct mill_type *type,
    struct mill_loop *loop,
    struct mill_cfh *parent,
    int flags)
{
    struct mill_cfh *sibling;

    self->type = type;
    self->state = 0;
    self->flags = flags;
    self->err = 0;
    self->parent = parent;
    self->children = 0;
    self->next = 0;
    self->prev = 0;
    self->loop = loop;

    /* Add this coroutine to the paren't list of children. */
    if (self->parent) {
        sibling = self->parent->children;
        self->next = sibling;
        if (sibling)
            sibling->prev = self;
        self->parent->children = self;
    }
}

void mill_cfh_getresult (struct mill_cfh *cfh, void **who, int *err)
{
    if (who)
        *who = (void*) cfh;
    if (err)
        *err = cfh->err;
}

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

void mill_cancel (void *cfptr)
{
    struct mill_cfh *self;

    self = (struct mill_cfh*) cfptr;
    assert (self->type->tag == mill_type_tag);
    self->type->handler (self, mill_event_term);
}

void *mill_typeof (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;
    assert (cfh->type && cfh->type->tag == mill_type_tag);
    return (void*) &cfh->type;
}

void mill_cfh_emit (
    struct mill_cfh *self)
{
    mill_loop_emit (self->loop, self);
}

void mill_cfh_cancelall (struct mill_cfh *self)
{
    struct mill_cfh *ch;

    for (ch = self->children; ch != 0; ch = ch->next)
        ch->type->handler (ch,  mill_event_term);
}

int mill_cfh_haschildren (struct mill_cfh *self)
{
    return self->children ? 1 : 0;
}

/******************************************************************************/
/*  coroutine msleep                                                          */
/******************************************************************************/

static void mill_handler_msleep (void *cfptr, void *event)
{
    int rc;
    struct mill_cf_msleep *cf;

    cf = (struct mill_cf_msleep*) cfptr;

    assert (event == mill_event_term);

    /* Cancel the timer. */
    rc = uv_timer_stop (&cf->timer);
    uv_assert (rc);
    cf->mill_cfh.err = ECANCELED;
    mill_cfh_emit (&cf->mill_cfh);
}

static void msleep_cb (
    uv_timer_t *timer)
{
    struct mill_cf_msleep *cf;

    cf = mill_cont (timer, struct mill_cf_msleep, timer);

    /* The coroutine is done. */
    mill_cfh_emit (&cf->mill_cfh);
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
    struct mill_cf_msleep *cf;
    int flags = 0;

    cf = (struct mill_cf_msleep*) cfptr;

    /* If needed, allocate the coframe for the coroutine. */
    if (!cf) {
        flags |= mill_flag_deallocate;
        cf = malloc (sizeof (struct mill_cf_msleep));
        assert (cf);
    }

    /* Fill in the coframe. */
    mill_cfh_init (&cf->mill_cfh, &mill_type_msleep, loop, parent, flags);
    uv_timer_init (&loop->uv_loop, &cf->timer);

    /* Launch the timer. */
    uv_timer_start(&cf->timer, msleep_cb, milliseconds, 0);

    return (void*) cf;
}

int msleep (int milliseconds)
{
    assert (0);
}

