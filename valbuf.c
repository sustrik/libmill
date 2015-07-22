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

#include "utils.h"
#include "valbuf.h"

#include <stdlib.h>

void mill_valbuf_init(struct mill_valbuf *self) {
    self->ptr = NULL;
    self->capacity = MILL_VALBUF_SIZE;
}

void mill_valbuf_term(struct mill_valbuf *self) {
    if(self->ptr)
        free(self->ptr);
}

void *mill_valbuf_alloc(struct mill_valbuf *self, size_t sz) {
    /* If there's enough capacity for the type available, return either
       dynamically allocated buffer, if present, or the static buffer. */
    if(self->capacity >= sz)
        return self->ptr ? self->ptr : self->buf;
    /* Allocate or grow the dynamic buffer to accommodate the type. */
    self->ptr = realloc(self->ptr, sz);
    mill_assert(self->ptr);
    self->capacity = sz;
    return self->ptr;
}

void *mill_valbuf_get(struct mill_valbuf *self) {
    return self->ptr ? self->ptr : self->buf;
}

