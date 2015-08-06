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

#ifndef MILL_VALBUF_INCLUDED
#define MILL_VALBUF_INCLUDED

#include <stddef.h>

/* Maximum size of an item in a channel that can be handled without
   extra memory allocation. */
#ifndef MILL_VALBUF_SIZE
#define MILL_VALBUF_SIZE 128
#endif

/* Arbitrarily large temporary buffer. */
struct mill_valbuf {
    void *ptr;
    size_t capacity;
    char buf[MILL_VALBUF_SIZE];
};

void mill_valbuf_init(struct mill_valbuf *self);
void mill_valbuf_term(struct mill_valbuf *self);

/* Makes sure that the buffer is at least sz bytes long and returns pointer
   to it. It may destroy previous content of the buffer. */
void *mill_valbuf_alloc(struct mill_valbuf *self, size_t sz);

/* Returns pointer to the previously allocated buffer. It keeps contents of
   the buffer intact. */
void *mill_valbuf_get(struct mill_valbuf *self);

#endif

