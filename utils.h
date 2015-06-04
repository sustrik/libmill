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

#ifndef MILL_UTILS_H_INCLUDED
#define MILL_UTILS_H_INCLUDED

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>

/* For now use longjmp. Replace by a different mechanism as needed. */
struct mill_ctx {
    jmp_buf jbuf;
};

#define mill_setjmp(ctx) setjmp((ctx)->jbuf)
#define mill_jmp(ctx) longjmp((ctx)->jbuf, 1)

/* Cause panic. */
void mill_panic(const char *text);

/*  Takes a pointer to a member variable and computes pointer to the structure
    that contains it. 'type' is type of the structure, not the member. */
#define mill_cont(ptr, type, member) \
    (ptr ? ((type*) (((char*) ptr) - offsetof(type, member))) : NULL)

/* Current time. Millisecond precision. */
uint64_t mill_now();

/* Compile-time assert. */
#define MILL_CT_ASSERT_HELPER2(prefix, line) \
    prefix##line
#define MILL_CT_ASSERT_HELPER1(prefix, line) \
    MILL_CT_ASSERT_HELPER2(prefix, line)
#define MILL_CT_ASSERT(x) \
    typedef int MILL_CT_ASSERT_HELPER1(ct_assert_,__COUNTER__) [(x) ? 1 : -1]

#endif

