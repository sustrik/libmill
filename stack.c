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
#include "utils.h"

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

/* We have to cache at least one stack, otherwise we would not be able
   to deallocate it properly. */
MILL_CT_ASSERT(MILL_MAX_CACHED_STACKS > 0);

static volatile int mill_stack_unoptimisable1 = 1;
static volatile void *mill_stack_unoptimisable2 = NULL;

/* A stack of unused coroutine stacks. This allows for extra-fast allocation
   of a new stack. The FIFO nature of this structure minimises cache misses.
   When the stack is cached its mill_slist_item is placed on its top rather
   then on the bottom. That way we minimise page misses. */
static int mill_num_cached_stacks = 0;
static struct mill_slist mill_cached_stacks = {0};

void *mill_allocstack(void) {
    if(!mill_slist_empty(&mill_cached_stacks)) {
        --mill_num_cached_stacks;
        return (void*)(mill_slist_pop(&mill_cached_stacks) + 1);
    }
    char *ptr = malloc(MILL_STACK_SIZE);
    assert(ptr);
    return ptr + MILL_STACK_SIZE;
}

void mill_freestack(void *stack) {
    /* Put the stack to the list of cached stacks. */
    struct mill_slist_item *item = ((struct mill_slist_item*)stack) - 1;
    mill_slist_push_back(&mill_cached_stacks, item);
    if(mill_num_cached_stacks < MILL_MAX_CACHED_STACKS) {
        ++mill_num_cached_stacks;
        return;
    }
    /* We can't deallocate the stack we are running on at the moment.
       Standard C free() is not required to work when it deallocates its
       own stack from underneath itself. Instead, we'll deallocate one of
       the unused cached stacks. */
    struct mill_slist_item *x = mill_slist_pop(&mill_cached_stacks);
    assert(x != item);   
    free(((char*)(x + 1)) - MILL_STACK_SIZE);
}

