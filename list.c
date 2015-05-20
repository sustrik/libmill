/*

  Copyright (c) 2015 Martin Sustrik

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

#include "list.h"

#include <assert.h>
#include <stddef.h>

void mill_list_init(struct mill_list *self)
{
    self->first = NULL;
    self->last = NULL;
}

void mill_list_term(struct mill_list *self)
{
    assert(!self->first);
    assert(!self->last);
}

int mill_list_empty(struct mill_list *self)
{
    return self->first ? 0 : 1;
}

struct mill_list_item *mill_list_begin(struct mill_list *self)
{
    return self->first;
}

struct mill_list_item *mill_list_end(struct mill_list *self)
{
    return NULL;
}

struct mill_list_item *mill_list_prev(struct mill_list *self,
    struct mill_list_item *it)
{
    if(!it)
        return self->last;
    assert(it->prev != MILL_LIST_NOTINLIST);
    return it->prev;
}

struct mill_list_item *mill_list_next(struct mill_list *self,
    struct mill_list_item *it)
{
    assert(it->next != MILL_LIST_NOTINLIST);
    return it->next;
}

void mill_list_insert(struct mill_list *self, struct mill_list_item *item,
    struct mill_list_item *it)
{
    assert(!mill_list_item_isinlist (item));

    item->prev = it ? it->prev : self->last;
    item->next = it;
    if(item->prev)
        item->prev->next = item;
    if(item->next)
        item->next->prev = item;
    if(!self->first || self->first == it)
        self->first = item;
    if(!it)
        self->last = item;
}

struct mill_list_item *mill_list_erase(struct mill_list *self,
    struct mill_list_item *item)
{
    struct mill_list_item *next;

    assert(mill_list_item_isinlist (item));

    if(item->prev)
        item->prev->next = item->next;
    else
        self->first = item->next;
    if(item->next)
        item->next->prev = item->prev;
    else
        self->last = item->prev;

    next = item->next;

    item->prev = MILL_LIST_NOTINLIST;
    item->next = MILL_LIST_NOTINLIST;

    return next;
}

void mill_list_item_init(struct mill_list_item *self)
{
    self->prev = MILL_LIST_NOTINLIST;
    self->next = MILL_LIST_NOTINLIST;
}

void mill_list_item_term(struct mill_list_item *self)
{
    assert(!mill_list_item_isinlist(self));
}

int mill_list_item_isinlist(struct mill_list_item *self)
{
    return self->prev == MILL_LIST_NOTINLIST ? 0 : 1;
}

