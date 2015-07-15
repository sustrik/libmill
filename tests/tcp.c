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

#include "../libmill.h"

void client(void) {
    tcpsock cs = tcpconnect("127.0.0.1:5555", -1);
    assert(cs);

    msleep(now() + 100);

    char buf[16];
    ssize_t sz = tcprecv(cs, buf, 3, -1);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');

    tcpsend(cs, "123\n45\n6789", 11);
    int rc = tcpflush(cs);
    assert(rc == 0);

    tcpclose(cs);
}

int main() {
    char buf[16];

    tcpsock ls = tcplisten("*:5555");
    assert(ls);

    go(client());

    tcpsock as = tcpaccept(ls, -1);

    /* Test deadline. */
    size_t sz = tcprecv(as, buf, sizeof(buf), now() + 10);
    assert (sz == 0 && errno == ETIMEDOUT);

    tcpsend(as, "ABC", 3);
    int rc = tcpflush(as);
    assert(rc == 0);

    sz = tcprecvuntil(as, buf, sizeof(buf), '\n', -1);
    assert(sz == 4);
    assert(buf[0] == '1' && buf[1] == '2' && buf[2] == '3' && buf[3] == '\n');
    sz = tcprecvuntil(as, buf, sizeof(buf), '\n', -1);
    assert(sz == 3);
    assert(buf[0] == '4' && buf[1] == '5' && buf[2] == '\n');
    sz = tcprecvuntil(as, buf, 3, '\n', -1);
    assert(sz == 0);
    assert(buf[0] == '6' && buf[1] == '7' && buf[2] == '8');

    tcpclose(as);
    tcpclose(ls);

    return 0;
}

