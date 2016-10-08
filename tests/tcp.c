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
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "../libmill.h"

coroutine void client(int port) {
    ipaddr addr = ipremote("127.0.0.1", port, 0, -1);
    tcpsock cs = tcpconnect(addr, -1);
    assert(cs);

    char ipstr[16] = {0};
    ipaddrstr(addr, ipstr);
    assert(errno == 0);
    assert(strcmp(ipstr, "127.0.0.1") == 0);

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

coroutine void client2(int port) {
    ipaddr addr = ipremote("127.0.0.1", port, 0, -1);
    tcpsock conn = tcpconnect(addr, -1);
    assert(conn);
    msleep(now() + 100);
    tcpclose(conn);
}


int main() {
    char buf[16];

    tcpsock ls = tcplisten(iplocal(NULL, 5555, 0), 10);
    assert(ls);

    go(client(5555));

    tcpsock as = tcpaccept(ls, -1);

    /* Test port. */
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

    /* Test tcprecvuntil. */
    sz = tcprecvuntil(as, buf, sizeof(buf), "\n", 1, -1);
    assert(sz == 4);
    assert(buf[0] == '1' && buf[1] == '2' && buf[2] == '3' && buf[3] == '\n');
    sz = tcprecvuntil(as, buf, sizeof(buf), "\n", 1, -1);
    assert(sz == 3);
    assert(buf[0] == '4' && buf[1] == '5' && buf[2] == '\n');
    sz = tcprecvuntil(as, buf, 3, "\n", 1, -1);
    assert(sz == 3);
    assert(buf[0] == '6' && buf[1] == '7' && buf[2] == '8');

    tcpshutdown(as, SHUT_RDWR);
    tcpclose(as);
    tcpclose(ls);

    /* Test whether libmill performs correctly when faced with TCP pushback. */
    ls = tcplisten(iplocal(NULL, 5555, 0), 10);
    go(client2(5555));
    as = tcpaccept(ls, -1);
    assert(as);
    char buffer[2048];
    while(1) {
        size_t sz = tcpsend(as, buffer, 2048, -1);
        if(errno == ECONNRESET)
            break;
        if(errno != 0) {
            fprintf(stderr, "errno=%d\n", errno);
            assert(0);
        }
        tcpflush(as, -1);
        if(errno == ECONNRESET)
            break;
        if(errno != 0) {
            fprintf(stderr, "errno=%d\n", errno);
            assert(0);
        }
        assert(errno == 0);
    }
    tcpclose(as);
    tcpclose(ls);

    return 0;
}

