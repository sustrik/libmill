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
#include <stdint.h>
#include <stdlib.h>

#include "slist.h"
#include "stack.h"
#include "utils.h"

/* Size of stack for all coroutines. In bytes. Default size is slightly smaller
   than page size to account for malloc's chunk header. */
size_t mill_stack_size = 256 * 1024 - 256;

/* Maximum number of unused cached stacks. Keep in mind that we can't
   deallocate the stack you are running on. Thus we need at least one cached
   stack. */
int mill_max_cached_stacks = 64;

/* A stack of unused coroutine stacks. This allows for extra-fast allocation
   of a new stack. The FIFO nature of this structure minimises cache misses.
   When the stack is cached its mill_slist_item is placed on its top rather
   then on the bottom. That way we minimise page misses. */
static int mill_num_cached_stacks = 0;
static struct mill_slist mill_cached_stacks = {0};

int mill_preparestacks(int count, size_t stack_size) {
    /* Purge the cached stacks. */
    while(1) {
        struct mill_slist_item *item = mill_slist_pop(&mill_cached_stacks);
        if(!item)
            break;
        free(((char*)(item + 1)) - mill_stack_size);
    }
    /* Now that there are no stacks allocated, we can adjust the stack size. */
    mill_stack_size = stack_size;
    /* Make sure that the stacks won't get deallocated even if they aren't used
       at the moment. */
    if(count > mill_max_cached_stacks)
        mill_max_cached_stacks = count;
    /* Allocate the new stacks. */
    int i;
    for(i = 0; i != count; ++i) {
        char *ptr = malloc(mill_stack_size);
        if(!ptr)
            break;
        ptr += mill_stack_size;
        struct mill_slist_item *item = ((struct mill_slist_item*)ptr) - 1;
        mill_slist_push_back(&mill_cached_stacks, item);
    }
    return i;
}

void *mill_allocstack(void) {
    if(!mill_slist_empty(&mill_cached_stacks)) {
        --mill_num_cached_stacks;
        return (void*)(mill_slist_pop(&mill_cached_stacks) + 1);
    }
    char *ptr = malloc(mill_stack_size);
    if(!ptr)
        mill_panic("not enough memory to allocate coroutine stack");
    return ptr + mill_stack_size;
}

void mill_freestack(void *stack) {
    /* Put the stack to the list of cached stacks. */
    struct mill_slist_item *item = ((struct mill_slist_item*)stack) - 1;
    mill_slist_push_back(&mill_cached_stacks, item);
    if(mill_num_cached_stacks < mill_max_cached_stacks) {
        ++mill_num_cached_stacks;
        return;
    }
    /* We can't deallocate the stack we are running on at the moment.
       Standard C free() is not required to work when it deallocates its
       own stack from underneath itself. Instead, we'll deallocate one of
       the unused cached stacks. */
    item = mill_slist_pop(&mill_cached_stacks);  
    free(((char*)(item + 1)) - mill_stack_size);
}

