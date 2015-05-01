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

#include <stdio.h>

void connect_socket(void) {
    int cs = msocket(AF_INET, SOCK_STREAM, 0);
    assert(cs != -1);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = 0x0100007f;
    addr.sin_port = htons(5555);
    int rc = mconnect(cs, (struct sockaddr*)&addr, sizeof(addr));
    assert(rc != -1);
    ssize_t sz = msend(cs, "ABC", 3, 0);
    assert(sz == 3);
    close(cs);
}

int main() {
    int ls = msocket(AF_INET, SOCK_STREAM, 0);
    assert(ls != -1);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(5555);
    int rc = bind(ls, (struct sockaddr*)&addr, sizeof(addr));
    assert(rc != -1);
    rc = listen(ls, 10);
    assert(rc != -1);
    go(connect_socket());
    int ac = maccept(ls, NULL, NULL);
    assert(ac >= 0);
    char buf[3];
    ssize_t sz = mrecv(ac, buf, sizeof(buf), 0);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    close(ac);
    return 0;
}

