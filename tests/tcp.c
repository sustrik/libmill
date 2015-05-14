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
#include <netinet/in.h>
#include <string.h>

#include "../mill.h"
#include "../tcp.h"

void connect_socket(void) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = 0x0100007f;
    addr.sin_port = htons(5555);
    tcpconn cs = tcpdial((struct sockaddr*)&addr, sizeof(addr));
    assert(cs);

    ssize_t sz = tcpwrite(cs, "ABC", 3);
    assert(sz == 0);

    char buf[16];
    sz = tcpreaduntil(cs, buf, sizeof(buf), '\n');
    assert(sz == 4);
    assert(buf[0] == '1' && buf[1] == '2' && buf[2] == '3' && buf[3] == '\n');
    sz = tcpreaduntil(cs, buf, sizeof(buf), '\n');
    assert(sz == 3);
    assert(buf[0] == '4' && buf[1] == '5' && buf[2] == '\n');

    tcpconn_close(cs);
}

int main() {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(5555);
    tcplistener ls = tcplisten((struct sockaddr*)&addr, sizeof(addr));
    assert(ls);

    go(connect_socket());

    tcpconn as = tcpaccept(ls);

    char buf[16];
    ssize_t sz = tcpread(as, buf, 3);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');

    sz = tcpwrite(as, "123\n45\n89", 9);
    assert(sz == 0);

    tcpconn_close(as);
    tcplistener_close(ls);

    return 0;
}

