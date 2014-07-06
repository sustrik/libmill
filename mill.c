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

/******************************************************************************/
/* Generic helpers.                                                           */
/******************************************************************************/

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

static void loop_close_cb (uv_handle_t *handle)
{
    struct mill_loop *self;

    self = mill_cont (handle, struct mill_loop, idle);
    uv_stop (&self->uv_loop);
}

static void loop_cb (uv_idle_t* handle)
{
    struct mill_loop *self;
    struct mill_cfh *src;

    self = mill_cont (handle, struct mill_loop, idle);

    while (self->first) {
        src = self->first;

        /* Copy the output arguments to their proper destinations. */
        src->type->output (src);

        /*  If top level coroutine exits, we can stop the event loop.
            However, first we have to close the 'idle' object. */
        if (!src->parent) {
            uv_close ((uv_handle_t*) &self->idle, loop_close_cb);
            return;
        }

        /* Invoke the handler for the finished coroutine. */
        src->parent->type->handler (src->parent, src);

        /* Move to the next event. */
        self->first = src->nextev;

        /* Mark the coframe as uninitialised. */
        src->type = 0;

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
        self->last->nextev = ev;
    ev->nextev = 0;
    self->last = ev;
}

/******************************************************************************/
/*  Helpers used to implement mill keywords.                                  */
/******************************************************************************/

void mill_emit (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;

    /* At this point all the child coroutines should be stopped. */
    assert (!cfh->children);

    /* Remove the corouine from the parent's list of children. */
    if (cfh->parent) {
        if (cfh->parent->children == cfh)
            cfh->parent->children = cfh->next;
        if (cfh->prev)
            cfh->prev->next = cfh->next;
        if (cfh->next)
            cfh->next->prev = cfh->prev;
        cfh->prev = 0;
        cfh->next = 0;
    }

    /* Add the coroutine to the event queue. */
    mill_loop_emit (cfh->loop, cfh);
}

void mill_add_child (void *cfptr, void *child)
{
    struct mill_cfh *cfh_parent;
    struct mill_cfh *cfh_child;

    cfh_parent = (struct mill_cfh*) cfptr;
    cfh_child = (struct mill_cfh*) child;

    cfh_child->prev = 0;
    cfh_child->next = cfh_parent->children;
    if (cfh_parent->children)
        cfh_parent->children->prev = cfh_child;
    cfh_parent->children = cfh_child;
}

void mill_who (void *cfptr, void **who)
{
    /* This simple operation is defined as a function rather than a macro
       so that 'who' argument is not evaluated twice. */
    if (who)
        *who = cfptr; 
}

void mill_cancel (void *cfptr, void *child)
{
    struct mill_cfh *cfh_parent;
    struct mill_cfh *cfh_child;

    cfh_parent = (struct mill_cfh*) cfptr;
    cfh_child = (struct mill_cfh*) child;

    /* Make sure that the handle passed by user actually corresponds
       to a valid child coroutine. */
    assert (cfh_child->type->tag == mill_type_tag);
    assert (cfh_child->parent == cfh_parent);

    /* Ask the coroutine to cancel. */
    cfh_child->type->handler (cfh_child, 0);
}

void mill_cancel_children (void *cfptr)
{
    struct mill_cfh *cfh;
    struct mill_cfh *child;

    cfh = (struct mill_cfh*) cfptr;

    /* Ask all child coroutines to cancel. */
    for (child = cfh->children; child != 0; child = child->next)
        child->type->handler (child, 0);
}

int mill_has_children (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;
    return cfh->children ? 1 : 0;
}

void *mill_typeof (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;

    /* Make sure that the handle specified by the user is a valid
       coroutine handle. */
    assert (cfh->type && cfh->type->tag == mill_type_tag);

    return (void*) cfh->type;
}

