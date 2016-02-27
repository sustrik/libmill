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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/*  Takes a pointer to a member variable and computes pointer to the structure
    that contains it. 'type' is type of the structure, not the member. */
#define mill_cont(ptr, type, member) \
    (ptr ? ((type*) (((char*) ptr) - offsetof(type, member))) : NULL)

/* Compile-time assert. */
#define MILL_CT_ASSERT_HELPER2(prefix, line) \
    prefix##line
#define MILL_CT_ASSERT_HELPER1(prefix, line) \
    MILL_CT_ASSERT_HELPER2(prefix, line)
#define MILL_CT_ASSERT(x) \
    typedef int MILL_CT_ASSERT_HELPER1(ct_assert_,__COUNTER__) [(x) ? 1 : -1]

#if defined __GNUC__ || defined __llvm__
#define mill_fast(x) __builtin_expect(!!(x), 1)
#define mill_slow(x) __builtin_expect(!!(x), 0)
#else
#define mill_fast(x) (x)
#define mill_slow(x) (x)
#endif

/* Define our own assert. This way we are sure that it stays in place even
   if the standard C assert would be thrown away by the compiler. */
#define mill_assert(x) \
    do {\
        if (mill_slow(!(x))) {\
            fprintf(stderr, "Assert failed: " #x " (%s:%d)\n",\
                __FILE__, __LINE__);\
            fflush(stderr);\
            abort();\
        }\
    } while (0)

#endif

