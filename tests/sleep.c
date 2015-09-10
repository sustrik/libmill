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
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

#include "../libmill.h"

coroutine static void delay(int n, chan ch) {
    msleep(now() + n);
    chs(ch, int, n);
}

int main() {
    /* Test 'msleep'. */
    int64_t deadline = now() + 100;
    msleep(deadline);
    int64_t diff = now () - deadline;
    assert(diff > -20 && diff < 20);

    /* msleep-sort */
    chan ch = chmake(int, 0);
    go(delay(30, ch));
    go(delay(40, ch));
    go(delay(10, ch));
    go(delay(20, ch));
    assert(chr(ch, int) == 10);
    assert(chr(ch, int) == 20);
    assert(chr(ch, int) == 30);
    assert(chr(ch, int) == 40);

    return 0;
}

