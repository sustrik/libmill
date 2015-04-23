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

#ifndef MILL_H_INCLUDED
#define MILL_H_INCLUDED

#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdint.h>

/******************************************************************************/
/* Coroutines                                                                 */
/******************************************************************************/

#define STACK_SIZE 16384

volatile int unoptimisable_ = 1;

struct cr_ {
    struct cr_ *next;
    char *stack;
    jmp_buf ctx;
    uint64_t expiry;
};

struct cr_ main_cr_ = {NULL};

struct cr_ *first_cr_ = &main_cr_;
struct cr_ *last_cr_ = &main_cr_;

/* go(fx(args)) executes fuction fx as a new coroutine. */
#define go(fn) \
    do {\
        if(!setjmp(first_cr_->ctx)) {\
            char *stack = malloc(STACK_SIZE);\
            char anchor_[unoptimisable_];\
            char filler_[&anchor_[0] - stack - STACK_SIZE];\
            struct cr_ cr[unoptimisable_];\
            cr->next = first_cr_;\
            first_cr_ = cr;\
            cr->stack = stack;\
            fn;\
            free(cr->stack);\
            first_cr_ = first_cr_->next;\
            longjmp(first_cr_->ctx, 1);\
        }\
    } while(0)

/* Yield CPU to a different coroutine. */
void yield(void);

void msleep(unsigned int sec);
void musleep(unsigned long us);

#endif

