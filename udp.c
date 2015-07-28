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

#include "libmill.h"
#include "net.h"
#include "utils.h"

#include <fcntl.h>
#include <unistd.h>

MILL_CT_ASSERT(sizeof(udpaddr) >= sizeof(struct sockaddr_in));
MILL_CT_ASSERT(sizeof(udpaddr) >= sizeof(struct sockaddr_in6));

struct mill_udpsock {
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

MILL_EXPORT udpaddr udpresolve(const char *addr, int port) {
    udpaddr result;
    socklen_t len = sizeof(result);
    int rc = mill_resolve(addr, port, (struct sockaddr_storage*) &result, &len);
    if (rc != 0)
        ((struct sockaddr*)&result)->sa_family = AF_UNSPEC;
    return result;
}

udpsock udplisten(const char *addr, int port) {
    struct sockaddr_storage ss;
    socklen_t len;
    int rc = mill_resolve(addr, port, &ss, &len);
    if (rc != 0)
        return NULL;

    /* Open the listening socket. */
    int s = socket(ss.ss_family, SOCK_DGRAM, 0);
    if(s == -1)
        return NULL;
    mill_udptune(s);

    /* Start listening. */
    rc = bind(s, (struct sockaddr*)&ss, len);
    if(rc != 0)
        return NULL;

    /* If the user requested an ephemeral port,
       retrieve the port number assigned by the OS now. */
    if(!port) {
        len = sizeof(ss);
        rc = getsockname(s, (struct sockaddr*)&ss, &len);
        if(rc == -1) {
            int err = errno;
            close(s);
            errno = err;
            return NULL;
        }
        if(ss.ss_family == AF_INET)
            port = ((struct sockaddr_in*)&ss)->sin_port;
        else if(ss.ss_family == AF_INET6)
            port = ((struct sockaddr_in6*)&ss)->sin6_port;
        else
            mill_assert(0);
    }

    /* Create the object. */
    struct mill_udpsock *us = malloc(sizeof(struct mill_udpsock));
    mill_assert(us);
    us->fd = s;
    us->port = port;
    errno = 0;
    return us;
}

int udpport(udpsock s) {
    return s->port;
}

size_t udpsend(udpsock s, udpaddr addr, const void *buf, size_t len,
      int64_t deadline) {
    struct sockaddr *saddr = (struct sockaddr*) &addr;
    while(1) {
        ssize_t ss = sendto(s->fd, buf, len, 0, saddr, saddr->sa_family ==
            AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
        if(ss == len)
            break;
        mill_assert(ss < 0);
        if(errno != EAGAIN && errno != EWOULDBLOCK)
            return 0;
        int rc = fdwait(s->fd, FDW_OUT, deadline);
        if(rc == 0) {
            errno = ETIMEDOUT;
            return 0;
        }
    }
    errno = 0;
    return len;
}

size_t udprecv(udpsock s, udpaddr *addr, void *buf, size_t len,
      int64_t deadline) {
    size_t ss;
    while(1) {
        socklen_t slen = sizeof(udpaddr);
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

void udpclose(udpsock s) {
    int rc = close(s->fd);
    mill_assert(rc == 0);
    free(s);
}

