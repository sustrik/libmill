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

#if defined HAVE_SSL

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../libmill.h"

#define CONN_ESTABLISHED 1
#define CONN_SUCCEEDED 2
#define CONN_FAILED 3

coroutine void statistics(chan ch) {
    int connections = 0;
    int active = 0;
    int failed = 0;
    
    while(1) {
        int op = chr(ch, int);

        if(op == CONN_ESTABLISHED)
            ++connections, ++active;
        else
            --active;
        if(op == CONN_FAILED)
            ++failed;

        printf("Process ID: %d\n", (int)getpid());
        printf("Total number of connections: %d\n", connections);
        printf("Active connections: %d\n", active);
        printf("Failed connections: %d\n\n", failed);
    }
}

coroutine void dialogue(sslsock as, chan ch) {
    chs(ch, int, CONN_ESTABLISHED);

    int64_t deadline = now() + 10000;

    sslsend(as, "What's your name?\r\n", 19, deadline);
    if(errno != 0)
        goto cleanup;
    sslflush(as, deadline);
    if(errno != 0)
        goto cleanup;

    char inbuf[256];
    size_t sz = sslrecvuntil(as, inbuf, sizeof(inbuf), "\r", 1, deadline);
    if(errno != 0)
        goto cleanup;

    inbuf[sz - 1] = 0;
    char outbuf[256];
    int rc = snprintf(outbuf, sizeof(outbuf), "Hello, %s!\r\n", inbuf);

    sz = sslsend(as, outbuf, rc, deadline);
    if(errno != 0)
        goto cleanup;
    sslflush(as, deadline);
    if(errno != 0)
        goto cleanup;

    cleanup:
    if(errno == 0)
        chs(ch, int, CONN_SUCCEEDED);
    else
        chs(ch, int, CONN_FAILED);
    sslclose(as);
}

int main(int argc, char *argv[]) {

    int port = 5555;
    int nproc = 1;
    if(argc > 1)
        port = atoi(argv[1]);
    if(argc > 2)
        nproc = atoi(argv[2]);

    ipaddr addr = iplocal(NULL, port, 0);
    sslsock ls = ssllisten(addr, "./cert.pem", "./key.pem", 10);
    if(!ls) {
        perror("Can't open listening socket");
        return 1;
    }

    int i;
    for (i = 0; i < nproc - 1; ++i) {
        pid_t pid = mfork();
        if(pid < 0) {
           perror("Can't create new process");
           return 1;
        }
        if(pid == 0) {
          break;
        }
    }

    chan ch = chmake(int, 0);
    go(statistics(ch));

    while(1) {
        sslsock as = sslaccept(ls, -1);
        if(!as)
            continue;
        go(dialogue(as, ch));
    }
}

#else

int main(void) {
    return 1;
}

#endif

