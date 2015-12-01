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
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include "../libmill.h"

static coroutine void connector(void) {
    tcpsock s = tcpconnect(iplocal("127.0.0.1", 5555, 0), -1);
    assert(s);
    char buf[1];
    tcprecv(s, buf, 1, -1);
    assert(0);
}

static coroutine void sender(long roundtrips) {
    tcpsock s = tcpconnect(iplocal("127.0.0.1", 5555, 0), -1);
    assert(s);
    char buf[1];
    int i;
    size_t nbytes;
    for(i = 0; i != roundtrips; ++i) {
        nbytes = tcprecv(s, buf, 1, -1);
        assert(errno == 0);
        assert(nbytes == 1);
        assert(buf[0] == 'A');
        nbytes = tcpsend(s, "A", 1, -1);
        assert(errno == 0);
        assert(nbytes == 1);
        tcpflush(s, -1);
        assert(errno == 0);
    }
}

int main(int argc, char *argv[]) {
    if(argc != 3) {
        printf("usage: c10k <parallel-connections> <roundtrip-count>\n");
        return 1;
    }
    long conns = atol(argv[1]);
    long roundtrips = atol(argv[2]);
    assert(conns >= 1);

    tcpsock ls = tcplisten(iplocal("127.0.0.1", 5555, 0), 10);
    assert(ls);
    int i;
    for(i = 0; i != conns - 1; ++i) {
        go(connector());
        tcpsock as = tcpaccept(ls, -1);
        assert(as);
    }
    go(sender(roundtrips));
    tcpsock as = tcpaccept(ls, -1);
    assert(as);

    int64_t start = now();

    char buf[1];
    size_t nbytes;
    for(i = 0; i != roundtrips; ++i) {
        nbytes = tcpsend(as, "A", 1, -1);
        assert(errno == 0);
        assert(nbytes == 1);
        tcpflush(as, -1);
        assert(errno == 0);
        nbytes = tcprecv(as, buf, 1, -1);
        assert(errno == 0);
        assert(nbytes == 1);
        assert(buf[0] == 'A');
    }

    int64_t stop = now();
    long duration = (long)(stop - start);
    long us = (duration * 1000) / roundtrips;

    printf("done %ld roundtrips in %f seconds\n",
        roundtrips, ((float)duration) / 1000);
    printf("duration of a single roundtrip: %ld us\n", us);
    printf("roundtrips per second: %ld\n",
        (long)(roundtrips * 1000 / duration));

    return 0;
}

