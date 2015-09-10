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

#include <assert.h>
#include <stddef.h>

#include "../libmill.h"

static int dummy1 = 0;
static int dummy2 = 0;

coroutine void worker(void) {
    /* Test whether newly created coroutine has CLS set to NULL. */
    assert(cls() == NULL);

    /* Check whether CLS remains unchanged after yielding control
       to the main coroutine. */
    setcls(&dummy2);
    assert(cls() == &dummy2);
    yield();
    assert(cls() == &dummy2);
}

int main() {
    /* Test whether CLS of the main coroutine is NULL on program startup. */
    assert(cls() == NULL);

    /* Basic functionality. */
    setcls(&dummy1);
    assert(cls() == &dummy1);

    /* Check whether CLS is not messed up by launching a new coroutine. */
    go(worker());
    assert(cls() == &dummy1);
    yield();

    /* Check whether CLS is not messed up when coroutine terminates. */
    assert(cls() == &dummy1);

    return 0;
}

