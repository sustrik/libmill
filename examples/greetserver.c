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

/*
    This is a simple TCP server, listening on port 5555. When you connect
    to it it will ask you for your name. After receiving the name it will
    greet you and close the connection. If the connection is inactive for
    10 seconds, it is closed.

    Here's an example user session:

        $ telnet 127.0.0.1 5555
        Trying 127.0.0.1...
        Connected to 127.0.0.1.
        Escape character is '^]'.
        What's your name?
        Bartholomaeus
        Hello, Bartholomaeus!
        Connection closed by foreign host.
*/

#include "../libmill.h"

#include <assert.h>
#include <stdio.h>

void dialogue(tcpsock s) {

    int64_t deadline = now() + 10000;

    size_t sz = tcpsend(s, "What's your name?\n", 18, deadline);
    if(errno != 0)
        goto done;
    tcpflush(s, -1);
    if(errno != 0)
        goto done;

    char inbuf[256];
    sz = tcprecvuntil(s, inbuf, sizeof(inbuf), '\r', deadline);
    if(errno != 0)
        goto done;
    inbuf[sz - 1] = 0;

    char outbuf[256];
    int rc = snprintf(outbuf, sizeof(outbuf), "Hello, %s!\n", inbuf);
    assert(rc <= sizeof(outbuf));
    sz = tcpsend(s, outbuf, rc, deadline);
    if(errno != 0)
        goto done;
    tcpflush(s, deadline);
    if(errno != 0)
        goto done;

done:
    tcpclose(s);
}

int main(void) {

    tcpsock ls = tcplisten(NULL, 5555);
    assert(ls);

    while(1) {
        tcpsock as = tcpaccept(ls, -1);
        assert(as);
        go(dialogue(as));
    }

    return 0;
}

