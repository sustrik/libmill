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

/******************************************************************************/
/*  Mill keywords.                                                            */
/******************************************************************************/

void mill_getresult (struct mill_cfh *cfh, void **who, int *err)
{
    if (who)
        *who = (void*) cfh;
    if (err)
        *err = cfh->err;
}

void mill_cancel (void *cf)
{
    struct mill_cfh *self;

    self = (struct mill_cfh*) cf;
    assert (self->type->tag == mill_type_tag);
    self->type->handler (self, mill_event_term);
}

void mill_cfh_emit (
    struct mill_cfh *self)
{
    assert (0);
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

static void mill_handler_msleep (void *coframe, void *event)
{
    assert (0);
}

const struct mill_type mill_type_msleep =
    {mill_type_tag, mill_handler_msleep};

void *mill_call_msleep (
    void *cf,
    const struct mill_type *type,
    struct mill_loop *loop,
    void *parent,
    int millseconds)
{
}

