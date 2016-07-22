/*

  Copyright (c) 2016 John E. Haque

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

#include "libmill.h"

#define PORT 5555 
#define NCLIENT 10

int read_a_line(sslsock c, char *buf, int64_t deadline) {
    int total = 0;
    for(;;) {
        assert(total < 256);
        int len = sslrecv(c, buf + total, 1, deadline);
        if (len <= 0)
            return -1;
        total += len;
        if (buf[total-1] == '\n')
            break;
    }
    return total;
}

coroutine void client(int num, ipaddr raddr) {
    msleep(now()+ 50);

    int64_t deadline = now() + 1000;
    sslsock c = sslconnect(raddr, deadline);
    assert(c);  /* TODO: check for error (errno) */

    /* ssl_handshake(cs, -1); */

    char req[256];
    sprintf(req, "%d: This is a test.\n", num);
    int reqlen = strlen(req);
    int rc = sslsend(c, req, reqlen, deadline);
    assert(rc == reqlen); /* TODO: check for error or partial write */
    char resp[256];
    int resplen = read_a_line(c, resp, deadline);
    if (resplen >= 0)
        fprintf(stderr, "Response: %.*s", resplen, resp);
    sslclose(c);
}
 
coroutine void serve_client(sslsock c) {
    char buf[256];
    int64_t deadline = now()+1000;
    int total = read_a_line(c, buf, deadline);
    if (total >= 0) {
        fprintf(stderr, "Request: %.*s", total, buf);
        int rc = sslsend(c, buf, total, deadline);
        assert(rc == total);   /* TODO: check error and partial write */
    }
    sslclose(c);
}


int main(void) {
    /* Only needed for a server */
    int rc = sslinit("./tests/cert.pem", "./tests/key.pem");
    if (rc == 0) {
        /* ERR_print_errors_fp(stderr); */
        fprintf(stderr, "ssl_serv_init(): initialization failed.\n");
        fprintf(stderr, "Use the following command to create a self-signed certificate:\n");
        fprintf(stderr, "openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 100 -nodes\n");
        exit(1);
    }

    ipaddr laddr, raddr;
    laddr = iplocal(NULL, PORT, 0);
    raddr = ipremote("127.0.0.1", PORT, 0, -1);
    assert(errno == 0);
    sslsock lsock = ssllisten(laddr, 32);
    assert(errno == 0);
    int i;
    for (i = 1; i <= NCLIENT; i++)
        go(client(i, raddr));
    while (1) {
        sslsock c = sslaccept(lsock, now() + 1000);
        if (! c) /* ETIMEDOUT */
            break;
        go(serve_client(c));
    }

    msleep(now() + 500);
    return 0;
}

#else

int main(void) {
    return 0;
}

#endif
