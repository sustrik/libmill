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
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "../libmill.h"

static uint64_t now() {
    struct timeval tv;
    int rc = gettimeofday(&tv, NULL);
    assert(rc == 0);
    return ((uint64_t)tv.tv_sec) * 1000 + (((uint64_t)tv.tv_usec) / 1000);
}

void trigger(int fd, uint64_t ms) {
    msleep(ms);
    ssize_t sz = send(fd, "A", 1, 0);
    assert(sz == 1);
}

int main() {
    /* Create a pair of file descriptors for testing. */
    int fds[2];
    int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    assert(rc == 0);

    /* Check for out. */
    rc = fdwait(fds[0], FDW_OUT, NULL);
    assert(rc & FDW_OUT);
    assert(!(rc & ~FDW_OUT));

    /* Check with the timeout that doesn't expire. */
    uint64_t tm = 100;
    rc = fdwait(fds[0], FDW_OUT, &tm);
    assert(rc);
    assert(rc & FDW_OUT);
    assert(!(rc & ~FDW_OUT));

    /* Check with the timeout that does expire. */
    uint64_t ms = now();
    tm = 100;
    rc = fdwait(fds[0], FDW_IN, &tm);
    assert(rc == 0);
    ms = now() - ms;
    assert(ms > 90 && ms < 110);

    /* Check for in. */
    ssize_t sz = send(fds[1], "A", 1, 0);
    assert(sz == 1);
    rc = fdwait(fds[0], FDW_IN, NULL);
    assert(rc & FDW_IN);
    assert(!(rc & ~FDW_IN));

    /* Check for both in and out. */
    rc = fdwait(fds[0], FDW_IN | FDW_OUT, NULL);
    assert(rc & FDW_IN);
    assert(rc & FDW_OUT);
    assert(!(rc & ~(FDW_IN | FDW_OUT)));
    char c;
    sz = recv(fds[0], &c, 1, 0);
    assert(sz == 1);

    /* Check complex timeout scenario. */
    ms = now();
    go(trigger(fds[0], 30));
    tm = 90;
    rc = fdwait(fds[1], FDW_IN, &tm);
    assert(rc == FDW_IN);
    uint64_t delay = now() - ms;
    assert(delay > 25 && delay < 35);
    sz = recv(fds[1], &c, 1, 0);
    assert(sz == 1);
    msleep(30);
    rc = fdwait(fds[1], FDW_IN, &tm);
    assert(rc == 0);
    delay = now() - ms;
    assert(delay > 85 && delay < 95);

    close(fds[0]);
    close(fds[1]);

    return 0;
}

