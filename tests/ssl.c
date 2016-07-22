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

#if defined HAVE_SSL

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "../libmill.h"

coroutine void client(int port) {
    ipaddr addr = ipremote("127.0.0.1", port, 0, -1);
    sslsock cs = sslconnect(addr, -1);
    assert(cs);

    char ipstr[16] = {0};
    ipaddrstr(addr, ipstr);
    assert(errno == 0);
    assert(strcmp(ipstr, "127.0.0.1") == 0);

    msleep(now() + 100);

    char buf[16];
    size_t sz = sslrecv(cs, buf, 3, -1);
    assert(sz == 3 && buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');

    sz = sslsend(cs, "123\n45\n6789", 11, -1);
    assert(sz == 11 && errno == 0);
    sslflush(cs, -1);
    assert(errno == 0);

    sslclose(cs);
}

coroutine void client2(int port) {
    ipaddr addr = ipremote("127.0.0.1", port, 0, -1);
    sslsock conn = sslconnect(addr, -1);
    assert(conn);
    msleep(now() + 100);
    sslclose(conn);
}


int main() {
    char buf[16];

    sslsock ls = ssllisten(iplocal(NULL, 5555, 0),
        "./tests/cert.pem", "./tests/key.pem", 10);
    assert(ls);

    go(client(5555));

    sslsock as = sslaccept(ls, -1);

    /* Test port. */
    assert(sslport(as) != 5555);

    /* Test deadline. */
    int64_t deadline = now() + 30;
    size_t sz = sslrecv(as, buf, sizeof(buf), deadline);
    assert(sz == 0 && errno == ETIMEDOUT);
    int64_t diff = now() - deadline;
    assert(diff > -20 && diff < 20);

    sz = sslsend(as, "ABC", 3, -1);
    assert(sz == 3 && errno == 0);
    sslflush(as, -1);
    assert(errno == 0);

    /* Test sslrecvuntil. */
    sz = sslrecvuntil(as, buf, sizeof(buf), "\n", 1, -1);
    assert(sz == 4);
    assert(buf[0] == '1' && buf[1] == '2' && buf[2] == '3' && buf[3] == '\n');
    sz = sslrecvuntil(as, buf, sizeof(buf), "\n", 1, -1);
    assert(sz == 3);
    assert(buf[0] == '4' && buf[1] == '5' && buf[2] == '\n');
    sz = sslrecvuntil(as, buf, 3, "\n", 1, -1);
    assert(sz == 3);
    assert(buf[0] == '6' && buf[1] == '7' && buf[2] == '8');

    sslclose(as);
    sslclose(ls);

    /* Test whether libmill performs correctly when faced with TCP pushback. */
    ls = ssllisten(iplocal(NULL, 5555, 0),
        "./tests/cert.pem", "./tests/key.pem", 10);
    go(client2(5555));
    as = sslaccept(ls, -1);
    assert(as);
    char buffer[2048];
    while(1) {
        size_t sz = sslsend(as, buffer, 2048, -1);
        if(errno == ECONNRESET)
            break;
        if(errno != 0) {
            fprintf(stderr, "errno=%d\n", errno);
            assert(0);
        }
        sslflush(as, -1);
        if(errno == ECONNRESET)
            break;
        if(errno != 0) {
            fprintf(stderr, "errno=%d\n", errno);
            assert(0);
        }
        assert(errno == 0);
    }
    sslclose(as);
    sslclose(ls);

    return 0;
}

#else

int main(void) {
    return 0;
}

#endif

