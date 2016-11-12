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

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ip.h"
#include "libmill.h"
#include "utils.h"

struct mill_udpsock_ {
    int fd;
    int port;
};

static void mill_udptune(int s) {
    /* Make the socket non-blocking. */
    int opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    int rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    mill_assert(rc != -1);
}

struct mill_udpsock_ *mill_udplisten_(ipaddr addr) {
    /* Open the listening socket. */
    int s = socket(mill_ipfamily(addr), SOCK_DGRAM, 0);
    if(s == -1)
        return NULL;
    mill_udptune(s);

    /* Start listening. */
    int rc = bind(s, (struct sockaddr*)&addr, mill_iplen(addr));
    if(rc != 0)
        return NULL;

    /* If the user requested an ephemeral port,
       retrieve the port number assigned by the OS now. */
    int port = mill_ipport(addr);
    if(!port) {
        ipaddr baddr;
        socklen_t len = sizeof(ipaddr);
        rc = getsockname(s, (struct sockaddr*)&baddr, &len);
        if(rc == -1) {
            int err = errno;
            fdclean(s);
            close(s);
            errno = err;
            return NULL;
        }
        port = mill_ipport(baddr);
    }

    /* Create the object. */
    struct mill_udpsock_ *us = malloc(sizeof(struct mill_udpsock_));
    if(!us) {
        fdclean(s);
        close(s);
        errno = ENOMEM;
        return NULL;
    }
    us->fd = s;
    us->port = port;
    errno = 0;
    return us;
}

int mill_udpport_(struct mill_udpsock_ *s) {
    return s->port;
}

void mill_udpsend_(struct mill_udpsock_ *s, ipaddr addr,
        const void *buf, size_t len) {
    struct sockaddr *saddr = (struct sockaddr*) &addr;
    ssize_t ss = sendto(s->fd, buf, len, 0, saddr, saddr->sa_family ==
        AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
    if(mill_fast(ss == (ssize_t)len)) {
        errno = 0;
        return;
    }
    mill_assert(ss < 0);
    if(errno == EAGAIN || errno == EWOULDBLOCK)
        errno = 0;
}

size_t mill_udprecv_(struct mill_udpsock_ *s, ipaddr *addr,
      void *buf, size_t len, int64_t deadline) {
    ssize_t ss;
    while(1) {
        socklen_t slen = sizeof(ipaddr);
        ss = recvfrom(s->fd, buf, len, 0,
            (struct sockaddr*)addr, &slen);
        if(ss >= 0)
            break;
        if(errno != EAGAIN && errno != EWOULDBLOCK)
            return 0;
        int rc = fdwait(s->fd, FDW_IN, deadline);
        if(rc == 0) {
            errno = ETIMEDOUT;
            return 0;
        }
    }
    errno = 0;
    return (size_t)ss;
}

void mill_udpclose_(struct mill_udpsock_ *s) {
    fdclean(s->fd);
    int rc = close(s->fd);
    mill_assert(rc == 0);
    free(s);
}

