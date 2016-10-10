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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "../libmill.h"

int main() {
    udpsock s1 = udplisten(iplocal(NULL, 5555, 0));
    assert(s1);
    udpsock s2 = udplisten(iplocal(NULL, 5556, 0));
    assert(s2);

    char ipstr[IPADDR_MAXSTRLEN] = {0};
    ipaddrstr(ipremote("216.58.217.46", 5556, 0, -1), ipstr);
    assert(errno == 0);
    assert(strcmp(ipstr, "216.58.217.46") == 0);

    ipaddr addr = ipremote("127.0.0.1", 5556, 0, -1);

    while(1) {
        udpsend(s1, addr, "ABC", 3);
        assert(errno == 0);

        char buf[3];
        size_t sz = udprecv(s2, &addr, buf, sizeof(buf), now() + 100);
        if(errno == ETIMEDOUT)
            continue;
        assert(errno == 0);
        assert(sz == 3);
        break;
    }

    while(1) {
        udpsend(s2, addr, "DEF", 3);
        assert(errno == 0);

        char buf[3];
        size_t sz = udprecv(s1, &addr, buf, sizeof(buf), now() + 100);
        if(errno == ETIMEDOUT)
            continue;
        assert(errno == 0);
        assert(sz == 3);
        break;
    }

    udpclose(s2);
    udpclose(s1);
    
    return 0;
}

