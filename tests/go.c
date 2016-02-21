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

#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <sys/wait.h>

#include "../libmill.h"

int sum = 0;

coroutine void worker(int count, int n) {
    int i;
    for(i = 0; i != count; ++i) {
        sum += n;
        yield();
    }
}

coroutine void dummy(void) {
    msleep(now() + 50);
}

int main() {
    goprepare(10, 25000, 300);

    /* Try few coroutines with pre-prepared stacks. */
    assert(errno == 0);
    go(worker(3, 7));
    go(worker(1, 11));
    go(worker(2, 5));
    msleep(100);
    assert(sum == 42);

    /* Test whether stack deallocation works. */
    int i;
    for(i = 0; i != 20; ++i)
        go(dummy());
    msleep(now() + 100);

    /* Try to fork the process. */
    pid_t pid = mfork();
    assert(pid != -1);
    if(pid > 0) {
        int status;
        pid = waitpid(pid, &status, 0);
        assert(pid != -1);
        assert(WIFEXITED(status));
    }

    return 0;
}

