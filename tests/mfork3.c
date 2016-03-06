/*

  Copyright (c) 2016 Martin Sustrik

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
#include <unistd.h>

#include "../libmill.h"

int event = 0;

coroutine void worker(int fd) {
    int rc = fdwait(fd, FDW_IN, -1);
    assert(rc == FDW_IN);
    event = 1;
}

int main() {
    int fds[2];
    int rc = pipe(fds);
    assert(rc == 0);
    go(worker(fds[0]));
    /* Fork. */
    pid_t pid = mfork();
    assert(pid != -1);
    /* Parent waits for the child. */
    if(pid > 0) {
        ssize_t sz = write(fds[1], "A", 1);
        assert(sz == 1);
        int status;
        pid = waitpid(pid, &status, 0);
        assert(pid != -1);
        assert(WIFEXITED(status));
        assert(WEXITSTATUS(status) == 0);
        return 0;
    }

    /* Child waits to see whether the timer was properly removed. */
    msleep(now() + 200);
    assert(!event);

    return 0;
}

