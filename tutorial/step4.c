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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../libmill.h"

coroutine void dialogue(tcpsock as) {
    tcpsend(as, "What's your name?\r\n", 19, -1);
    if(errno != 0)
        goto cleanup;
    tcpflush(as, -1);
    if(errno != 0)
        goto cleanup;

    char inbuf[256];
    size_t sz = tcprecvuntil(as, inbuf, sizeof(inbuf), "\r", 1, -1);
    if(errno != 0)
        goto cleanup;

    inbuf[sz - 1] = 0;
    char outbuf[256];
    int rc = snprintf(outbuf, sizeof(outbuf), "Hello, %s!\r\n", inbuf);

    sz = tcpsend(as, outbuf, rc, -1);
    if(errno != 0)
        goto cleanup;
    tcpflush(as, -1);
    if(errno != 0)
        goto cleanup;

    cleanup:
    tcpclose(as);
}

int main(int argc, char *argv[]) {

    int port = 5555;
    if(argc > 1)
        port = atoi(argv[1]);

    ipaddr addr = iplocal(NULL, port, 0);
    tcpsock ls = tcplisten(addr, 10);
    if(!ls) {
        perror("Can't open listening socket");
        return 1;
    }

    while(1) {
        tcpsock as = tcpaccept(ls, -1);
        if(!as)
            continue;
        go(dialogue(as));
    }
}

