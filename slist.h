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

#ifndef MILL_SLIST_INCLUDED
#define MILL_SLIST_INCLUDED

/* Singly-linked list. Having both push and push_back functions means that
   it can be used both as a queue and as a stack. */

struct mill_slist_item {
    struct mill_slist_item *next;
};

struct mill_slist {
    struct mill_slist_item *first;
    struct mill_slist_item *last;
};

/* Initialise the list. To statically initialise the list use = {0}. */
void mill_slist_init(struct mill_slist *self);

/* True is the list has no items. */
#define mill_slist_empty(self) (!((self)->first))

/* Returns iterator to the first item in the list or NULL if
   the list is empty. */
#define mill_slist_begin(self) ((self)->first)

/* Returns iterator to one past the item pointed to by 'it'.
   If there are no more items returns NULL. */
#define mill_slist_next(it) ((it)->next)

/* Push the item to the beginning of the list. */
void mill_slist_push(struct mill_slist *self, struct mill_slist_item *item);

/* Push the item to the end of the list. */
void mill_slist_push_back(struct mill_slist *self,
    struct mill_slist_item *item);

/* Pop an item from the beginning of the list. */
struct mill_slist_item *mill_slist_pop(struct mill_slist *self);

#endif

