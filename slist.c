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

#include <stddef.h>

#include "slist.h"

void mill_slist_init(struct mill_slist *self) {
    self->first = NULL;
    self->last = NULL;
}

void mill_slist_push(struct mill_slist *self, struct mill_slist_item *item) {
    item->next = self->first;
    self->first = item;
    if(!self->last)
        self->last = item;
}

void mill_slist_push_back(struct mill_slist *self,
      struct mill_slist_item *item) {
    item->next = NULL;
    if(!self->last)
        self->first = item;
    else
        self->last->next = item;
    self->last = item;
}

struct mill_slist_item *mill_slist_pop(struct mill_slist *self) {
    if(!self->first)
        return NULL;
    struct mill_slist_item *it = self->first;
    self->first = self->first->next;
    if(!self->first)
        self->last = NULL;
    return it;
}

