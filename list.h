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

#ifndef MILL_LIST_INCLUDED
#define MILL_LIST_INCLUDED

struct mill_list_item {
    struct mill_list_item *next;
    struct mill_list_item *prev;
};

struct mill_list {
    struct mill_list_item *first;
    struct mill_list_item *last;
};

/* Initialise the list. To statically initialise the list use = {0}. */
void mill_list_init(struct mill_list *self);

/* Terminates the list. Note that all items must be removed before the
   termination. */
void mill_list_term(struct mill_list *self);

/* Returns 1 is list has zero items, 0 otherwise. */
int mill_list_empty(struct mill_list *self);

/* Returns iterator to the first item in the list. */
struct mill_list_item *mill_list_begin(struct mill_list *self);

/* Returns iterator to one past the last item in the list. */
struct mill_list_item *mill_list_end(struct mill_list *self);

/* Returns iterator to an item prior to the one pointed to by 'it'. */
struct mill_list_item *mill_list_prev(struct mill_list *self,
    struct mill_list_item *it);

/* Returns iterator to one past the item pointed to by 'it'. */
struct mill_list_item *mill_list_next(struct mill_list *self,
    struct mill_list_item *it);

/* Adds the item to the list before the item pointed to by 'it'.
   Prior to insertion item should not be part of any list. */
void mill_list_insert(struct mill_list *self, struct mill_list_item *item,
    struct mill_list_item *it);

/* Removes the item from the list and returns pointer to the next item in the
   list. Item must be part of the list. */
struct mill_list_item *mill_list_erase(struct mill_list *self,
    struct mill_list_item *item);

#endif

