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

#include "slist.h"
#include "stack.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

/* Size of stack for new coroutines. In bytes. */
#ifndef MILL_STACK_SIZE
#define MILL_STACK_SIZE (256 * 1024)
#endif

/* Maximum number of unused cached stacks. */
#ifndef MILL_MAX_CACHED_STACKS
#define MILL_MAX_CACHED_STACKS 64
#endif

/* 8 bytes written to the bottom of the stack to guard for stack overflows. */
#define MILL_STACK_GUARD 0xdeadbeefbadcafe0

/* A stack of unused coroutine stacks. This allows for extra-fast allocation
   of a new stack. The FIFO nature of this structure minimises cache misses.
   When the stack is cached its mill_slist_item is placed on its top rather
   then on the bottom. That way we minimise page misses. */
static int mill_num_cached_stacks = 0;
static struct mill_slist mill_cached_stacks = {0};

void *mill_allocstack(void) {
    if(!mill_slist_empty(&mill_cached_stacks))
        return (void*)(mill_slist_pop(&mill_cached_stacks) + 1);
    char *ptr = malloc(MILL_STACK_SIZE);
    assert(ptr);
    *((uint64_t*)ptr) = MILL_STACK_GUARD;
    return ptr + MILL_STACK_SIZE; 
}

void mill_freestack(void *stack) {
    if(mill_num_cached_stacks >= MILL_MAX_CACHED_STACKS) {
        char *ptr = ((char*)stack) - MILL_STACK_SIZE;
        free(ptr);
        return;
    }
    struct mill_slist_item *item = ((struct mill_slist_item*)stack) - 1;
    mill_slist_push(&mill_cached_stacks, item);
    ++mill_num_cached_stacks;
}

void mill_checkstack(void *stack, void *ptr) {
    char *bottom = ((char*) stack) - MILL_STACK_SIZE;
    assert (ptr <= stack && ptr > (void*)(bottom + 8) &&
        *((uint64_t*)bottom) == MILL_STACK_GUARD);
}

