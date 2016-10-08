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
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>

#define MILL_USE_PREFIX
#include "../libmill.h"

mill_coroutine void client(const char *addr) {
    mill_unixsock cs = mill_unixconnect(addr);
    assert(cs);

    mill_msleep(mill_now() + 100);

    char buf[16];
    size_t sz = mill_unixrecv(cs, buf, 3, -1);
    assert(sz == 3 && buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');

    sz = mill_unixsend(cs, "123\n45\n6789", 11, -1);
    assert(sz == 11 && errno == 0);
    mill_unixflush(cs, -1);
    assert(errno == 0);

    mill_unixshutdown(cs, SHUT_RDWR);
    mill_unixclose(cs);
}

int main() {
    const char *sockname = "milltest.sock";
    char buf[16];
    struct stat st;

    if (stat(sockname, &st) == 0) {
        assert(unlink(sockname) == 0);
    }

    mill_unixsock ls = mill_unixlisten(sockname, 10);
    assert(ls);

    mill_go(client(sockname));

    mill_unixsock as = mill_unixaccept(ls, -1);

    /* Test deadline. */
    int64_t deadline = mill_now() + 30;
    size_t sz = mill_unixrecv(as, buf, sizeof(buf), deadline);
    assert(sz == 0 && errno == ETIMEDOUT);
    int64_t diff = mill_now() - deadline;
    assert(diff > -20 && diff < 20);

    sz = mill_unixsend(as, "ABC", 3, -1);
    assert(sz == 3 && errno == 0);
    mill_unixflush(as, -1);
    assert(errno == 0);

    sz = mill_unixrecvuntil(as, buf, sizeof(buf), "\n", 1, -1);
    assert(sz == 4);
    assert(buf[0] == '1' && buf[1] == '2' && buf[2] == '3' && buf[3] == '\n');
    sz = mill_unixrecvuntil(as, buf, sizeof(buf), "\n", 1, -1);
    assert(sz == 3);
    assert(buf[0] == '4' && buf[1] == '5' && buf[2] == '\n');
    sz = mill_unixrecvuntil(as, buf, 3, "\n", 1, -1);
    assert(sz == 3);
    assert(buf[0] == '6' && buf[1] == '7' && buf[2] == '8');

    mill_unixclose(as);
    mill_unixclose(ls);
    unlink(sockname);

    mill_unixsock a, b;
    mill_unixpair(&a, &b);
    assert(errno == 0);

    sz = mill_unixsend(a, "ABC", 3, -1);
    assert(sz == 3 && errno == 0);
    mill_unixflush(a, -1);
    assert(errno == 0);
    sz = mill_unixrecv(b, buf, 3, -1);
    assert(sz == 3 && buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C' &&
        errno == 0);

    mill_unixclose(a);
    mill_unixclose(b);

    return 0;
}

