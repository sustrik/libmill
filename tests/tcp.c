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
#include <stdlib.h>

#include "../libmill.h"

coroutine void client(int port) {
    ipaddr addr = ipremote("127.0.0.1", port, 0, -1);
    tcpsock cs = tcpconnect(addr, -1);
    assert(cs);

    char ipstr[16] = {0};
    ipaddrstr(addr, ipstr);
    assert(errno == 0);
    assert(strcmp(ipstr, "127.0.0.1") == 0);

    int fd = tcpdetach(cs);
    assert(fd != -1);
    cs = tcpattach(fd, 0);
    assert(cs);

    msleep(now() + 100);

    char buf[16];
    size_t sz = tcprecv(cs, buf, 3, -1);
    assert(sz == 3 && buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');

    sz = tcpsend(cs, "123\n45\n6789", 11, -1);
    assert(sz == 11 && errno == 0);
    tcpflush(cs, -1);
    assert(errno == 0);

    tcpclose(cs);
}

int main() {
    char buf[16];

    tcpsock ls = tcplisten(iplocal(NULL, 5555, 0), 10);
    assert(ls);

    int fd = tcpdetach(ls);
    assert(fd != -1);
    ls = tcpattach(fd, 1);
    assert(ls);
    assert(tcpport(ls) == 5555);

    go(client(5555));

    tcpsock as = tcpaccept(ls, -1);

    /* Test retrieving address and port. */
    ipaddr addr = tcpaddr(as);
    char ipstr[IPADDR_MAXSTRLEN];
    assert(strcmp(ipaddrstr(addr, ipstr), "127.0.0.1") == 0);
    assert(tcpport(as) != 5555);

    /* Test deadline. */
    int64_t deadline = now() + 30;
    size_t sz = tcprecv(as, buf, sizeof(buf), deadline);
    assert(sz == 0 && errno == ETIMEDOUT);
    int64_t diff = now() - deadline;
    assert(diff > -20 && diff < 20);

    sz = tcpsend(as, "ABC", 3, -1);
    assert(sz == 3 && errno == 0);
    tcpflush(as, -1);
    assert(errno == 0);

    sz = tcprecvuntil(as, buf, sizeof(buf), "\n", 1, -1);
    assert(sz == 4);
    assert(buf[0] == '1' && buf[1] == '2' && buf[2] == '3' && buf[3] == '\n');
    sz = tcprecvuntil(as, buf, sizeof(buf), "\n", 1, -1);
    assert(sz == 3);
    assert(buf[0] == '4' && buf[1] == '5' && buf[2] == '\n');
    sz = tcprecvuntil(as, buf, 3, "\n", 1, -1);
    assert(sz == 3);
    assert(buf[0] == '6' && buf[1] == '7' && buf[2] == '8');

    tcpclose(as);
    tcpclose(ls);

    return 0;
}

