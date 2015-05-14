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
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "tcp.h"
#include "mill.h"

#define MILL_TCP_LISTEN_BACKLOG 10

#define MILL_TCP_BUFLEN 1500

struct tcplistener {
    int fd;
};

struct tcpconn {
    int fd;
    int first;
    int len;
    char buf[MILL_TCP_BUFLEN];
};

static struct tcpconn *tcpconn_create(int fd) {
    struct tcpconn *conn = malloc(sizeof(struct tcpconn));
    assert(conn);
    conn->fd = fd;
    conn->first = 0;
    conn->len = 0;
    return conn;
}

tcplistener tcplisten(const struct sockaddr *addr, socklen_t addrlen) {
    /* Open the listening socket. */
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if(s == -1)
        return NULL;

    /* Make it non-blocking. */
    int opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    int rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    assert(rc != -1);

    /* Start listening. */
    rc = bind(s, addr, addrlen);
    assert(rc != -1);
    rc = listen(s, MILL_TCP_LISTEN_BACKLOG);
    assert(rc != -1);

    /* Create the object. */
    struct tcplistener *listener = malloc(sizeof(struct tcplistener));
    assert(listener);
    listener->fd = s;
    return listener;
}

tcpconn tcpaccept(tcplistener listener) {
	while(1) {
        /* Try to get new connection (non-blocking). */
        int s = accept(listener->fd, NULL, NULL);
        if (s >= 0) {
            /* Put the newly created connection into non-blocking mode. */
            int opt = fcntl(s, F_GETFL, 0);
            if (opt == -1)
                opt = 0;
            int rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
            assert(rc != -1);

            /* Create the object. */
			return tcpconn_create(s);
        }
        assert(s == -1);
        if(errno != EAGAIN && errno != EWOULDBLOCK)
            return NULL;
        /* Wait till new connection is available. */
        int rc = fdwait(listener->fd, FDW_IN);
        assert(rc == FDW_IN);
    }
}

void tcplistener_close(tcplistener listener) {
    int rc = close(listener->fd);
    assert(rc == 0);
    free(listener);
}

tcpconn tcpdial(const struct sockaddr *addr, socklen_t addrlen) {
    /* Open a socket. */
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if(s == -1)
        return NULL;

    /* Make it non-blocking. */
    int opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    int rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    assert(rc != -1);

    /* Connect to the remote endpoint. */
    rc = connect(s, addr, addrlen);
    if(rc != 0) {
		assert(rc == -1);
		if(errno != EINPROGRESS)
		    return NULL;
		rc = fdwait(s, FDW_OUT);
        assert(rc == FDW_OUT);
		/* TODO: Handle errors. */
    }

    /* Create the object. */
	return tcpconn_create(s);
}

void tcpconn_close(tcpconn conn) {
    int rc = close(conn->fd);
    assert(rc == 0);
    free(conn);
}

ssize_t tcpwrite(tcpconn conn, const void *buf, size_t len) {
    char *pos = (char*)buf;
    size_t remaining = len;
    while(remaining) {
        ssize_t sz = send(conn->fd, pos, remaining, 0);
        if(sz == -1) {
            if(errno != EAGAIN && errno != EWOULDBLOCK)
                return -1;
            int rc = fdwait(conn->fd, FDW_OUT);
            assert(rc == FDW_OUT);
            continue;
        }
        pos += sz;
        remaining -= sz;
    }
    return 0;
}

ssize_t tcpread(tcpconn conn, void *buf, size_t len) {
    /* If there's enough data in the buffer it's easy. */
    if(conn->len >= len) {
        memcpy(buf, &conn->buf[conn->first], len);
        conn->first += len;
        conn->len -= len;
        return 0;
    }

    /* Let's move all the data from the buffer first. */
    char *pos = (char*)buf;
    size_t remaining = len;
    memcpy(pos, &conn->buf[conn->first], conn->len);
    pos += conn->len;
    remaining -= conn->len;
    conn->first = 0;
    conn->len = 0;

    assert(remaining);
    while(1) {
        if(remaining > MILL_TCP_BUFLEN) {
            /* If we still have a lot to read try to read it in one go directly
               into the destination buffer. */
            ssize_t sz = recv(conn->fd, pos, remaining, 0);
            assert(sz != 0); // TODO
            if(sz == -1) {
                assert(errno == EAGAIN && errno == EWOULDBLOCK); // TODO
                sz = 0;
            }
            if(sz == remaining)
                return 0;
            pos += sz;
            remaining -= sz;
        }
        else {
            /* If we have just a little to read try to read the full connection
               buffer to minimise the number of system calls. */    
            ssize_t sz = recv(conn->fd, conn->buf, MILL_TCP_BUFLEN, 0);
            assert(sz != 0); // TODO
            if(sz == -1) {
                assert(errno == EAGAIN && errno == EWOULDBLOCK); // TODO
                sz = 0;
            }
            if(sz < remaining) {
                memcpy(pos, conn->buf, sz);
                pos += sz;
                remaining -= sz;
                conn->first = 0;
                conn->len = 0;
            }
            else {
                memcpy(pos, conn->buf, remaining);
                conn->first = remaining;
                conn->len = sz - remaining;
                return 0;
            }
        }

        /* Wait till there's more data to read. */
        int res = fdwait(conn->fd, FDW_IN);
        assert(res == FDW_IN);
    }
}

ssize_t tcpreaduntil(tcpconn conn, void *buf, size_t len, char until) {
    char *pos = (char*)buf;
    size_t i;
    for(i = 0; i != len; ++i, ++pos) {
        ssize_t res = tcpread(conn, pos, 1);
        if (res != 0)
            return -1;
        if(*pos == until)
            return i + 1;
    }
    return 0;
}

