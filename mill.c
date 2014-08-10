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
/* Tracing support.                                                           */
/******************************************************************************/

static int mill_trace = 0;

void _mill_trace ()
{
    mill_trace = 1;
}

static mill_printstack (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;
    if (cfh->parent) {
        mill_printstack (cfh->parent);
        printf ("/");
    }
    printf ("%s", cfh->type->name);
}

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
    struct mill_cfh *dst;
    struct mill_cfh *it;
    struct mill_cfh *prev;

    self = mill_cont (handle, struct mill_loop, idle);

    while (self->first) {
        src = self->first;

        /*  If top level coroutine exits, we can stop the event loop.
            However, first we have to close the 'idle' object. */
        if (!src->parent) {

            /* Deallocate the coframe. */
            mill_coframe_term (src);
            free (src);

            uv_close ((uv_handle_t*) &self->idle, loop_close_cb);
            return;
        }

        dst = src->parent;

        /* Invoke the handler for the finished coroutine. If the event can't
           be processed immediately.... */
        if (dst->type->handler (dst, src) != 0) {

            /* Remove it from the loop's event queue. */
            self->first = src->nextev;

            /* And put it to the end of parent's pending event queue. */
            src->nextev = 0;
            if (dst->pfirst)
                dst->plast->nextev = src;
            else
                dst->pfirst = src;
            dst->plast = src;

            continue;
        }

        /* The event was processed successfully. That may have caused some of
           the pending events to become applicable. */
        it = dst->pfirst;
        prev = 0;
        while (it != 0) {

            /* Try to process the event. */
            if (dst->type->handler (dst, it) == 0) {

                /* Even was processed successfully. Remove it from the queue. */
                if (prev)
                    prev->nextev = it->nextev;
                else
                    dst->pfirst = it->nextev;
                if (!it->nextev)
                    dst->plast = prev;

                /* The coframe was already terminated by the handler.
                   Now deallocate the memory. */
                free (it);
                
                /* Start checking the pending event queue from beginning so
                   that older events are processed before newer ones. */
                it = dst->pfirst;
                prev = 0;
                continue;
            }

            /* If the event isn't applicable, try the next one. */
            prev = it;
            it = it->nextev;
        }

        /* Move to the next event. */
        self->first = src->nextev;

        /* The coframe was already terminated by the handler.
           Now deallocate the memory. */
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

void mill_coframe_init (
    void *cfptr,
    const struct mill_type *type,
    void *parent,
    struct mill_loop *loop)
{
    struct mill_cfh *cfh;
    struct mill_cfh *pcfh;

    cfh = (struct mill_cfh*) cfptr;

    /*  Initialise the coframe. */
    cfh->type = type;
    cfh->pc = 0;
    cfh->nextev = 0;
    cfh->pfirst = 0;
    cfh->plast = 0;
    cfh->parent = parent;
    cfh->children = 0;
    cfh->next = 0;
    cfh->prev = 0;
    cfh->loop = loop;

    /* Add the coframe to the parent's list of child coroutines. */
    if (parent) {
        pcfh = (struct mill_cfh*) parent;
        cfh->prev = 0;
        cfh->next = pcfh->children;
        if (pcfh->children)
            pcfh->children->prev = cfh;
        pcfh->children = cfh;
    }

    /* Trace start of the new coroutine. */
    if (mill_trace) {
        printf ("mill ==> go     ");
        mill_printstack (cfh);
        printf ("\n");
    }
}

void mill_coframe_term (
    void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;

    /* Tracing support. No need to trace deallocation of the top-level coframe
       as the return from the coroutine is already reported and the two are
       basically the same thing. */
    if (mill_trace && cfh->parent) {
        printf ("mill ==> select ");
        mill_printstack (cfh);
        printf ("\n");
    }

    /* Copy the 'out' arguments to their final destinations. */
    cfh->type->output (cfh);

    /* Remove the coframe from the paren't list of child coroutines. */
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

    /* This is a heuristic that should cause an error if deallocated
       coframe is used. */
    cfh->type = 0;
}

void mill_emit (
    void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;

    /* At this point all the child coroutines should be stopped. */
    assert (!cfh->children);

    /* Add the coroutine to the event queue. */
    mill_loop_emit (cfh->loop, cfh);

    if (mill_trace) {
        printf ("mill ==> return ");
        mill_printstack (cfh);
        printf ("\n");
    }
}

void mill_cancel_children (
    void *cfptr)
{
    struct mill_cfh *cfh;
    struct mill_cfh *child;

    cfh = (struct mill_cfh*) cfptr;

    /* Ask all child coroutines to cancel. */
    for (child = cfh->children; child != 0; child = child->next) {
        if (mill_trace) {
            printf ("mill ==> cancel ");
            mill_printstack (child);
            printf ("\n");
        }
        child->type->handler (child, 0);
    }
}

int mill_has_children (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;
    return cfh->children ? 1 : 0;
}

