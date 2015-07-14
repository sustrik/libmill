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
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "libmill.h"
#include "utils.h"

#define MILL_TCP_LISTEN_BACKLOG 10

#define MILL_TCP_BUFLEN 1500

enum mill_tcptype {
   MILL_TCPLISTENER,
   MILL_TCPCONN
};

struct tcpsock {
    enum mill_tcptype type;
};

struct tcplistener {
    struct tcpsock sock;
    int fd;
};

struct tcpconn {
    struct tcpsock sock;
    int fd;
    int ifirst;
    int ilen;
    int olen;
    int oerr;
    char ibuf[MILL_TCP_BUFLEN];
    char obuf[MILL_TCP_BUFLEN];
};

static struct tcpconn *tcpconn_create(int fd) {
    struct tcpconn *conn = malloc(sizeof(struct tcpconn));
    assert(conn);
    conn->sock.type = MILL_TCPCONN;
    conn->fd = fd;
    conn->ifirst = 0;
    conn->ilen = 0;
    conn->olen = 0;
    conn->oerr = 0;
    return conn;
}

static int resolve_ip4_literal_addr(const char *addr,
      struct sockaddr_in *addr_in) {
    memset(addr_in, 0, sizeof(struct sockaddr_in));
    addr_in->sin_family = AF_INET;
    char *colon = strrchr(addr, ':');

    /* Port. */
    int port = 0;
    if(colon) {
        const char *pos = colon + 1;
        while(*pos) {
          if(*pos < '0' || *pos > '9') {
            errno = EINVAL;
            return -1;
          }
          port *= 10;
          port += *pos - '0';
          ++pos;
        }
        if(port < 0 || port > 0xffff) {
            errno = EINVAL;
            return -1;
        }
    }
    addr_in->sin_port = htons(port);

    /* Get the IP address part of the string. */
    const char *straddr;
    char straddrbuf[256];
    if(!colon)
        straddr = addr;
    else {
        assert(colon - addr + 1 <= sizeof(straddrbuf));
        memcpy(straddrbuf, addr, colon - addr);
        straddrbuf[colon - addr] = 0;
        straddr = straddrbuf;
    }

    /* Wildcard address. */
    if(strcmp(straddr, "*") == 0) {
        addr_in->sin_addr.s_addr = INADDR_ANY;
        return 0;
    }

    /* IPv4 address. */
    int rc = inet_pton(AF_INET, straddr, &addr_in->sin_addr);
    if(rc != 1) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

tcpsock tcplisten(const char *addr) {
    struct sockaddr_in addr_in;
    int rc = resolve_ip4_literal_addr(addr, &addr_in);
    if (rc != 0)
        return NULL;

    /* Open the listening socket. */
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if(s == -1)
        return NULL;
    int opt = 1;
    rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
    assert(rc == 0);

    /* Make it non-blocking. */
    opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    assert(rc != -1);

    /* Start listening. */
    rc = bind(s, (struct sockaddr*)&addr_in, sizeof(addr_in));
    assert(rc != -1);
    rc = listen(s, MILL_TCP_LISTEN_BACKLOG);
    assert(rc != -1);

    /* Create the object. */
    struct tcplistener *l = malloc(sizeof(struct tcplistener));
    assert(l);
    l->sock.type = MILL_TCPLISTENER;
    l->fd = s;
    return &l->sock;
}

tcpsock tcpaccept(tcpsock s) {
    if(s->type != MILL_TCPLISTENER)
        mill_panic("trying to accept on a socket that isn't listening");
    struct tcplistener *l = (struct tcplistener*)s;
    while(1) {
        /* Try to get new connection (non-blocking). */
        int as = accept(l->fd, NULL, NULL);
        if (as >= 0) {
            /* Put the newly created connection into non-blocking mode. */
            int opt = fcntl(as, F_GETFL, 0);
            if (opt == -1)
                opt = 0;
            int rc = fcntl(as, F_SETFL, opt | O_NONBLOCK);
            assert(rc != -1);

            /* Create the object. */
            return &tcpconn_create(as)->sock;
        }
        assert(as == -1);
        if(errno != EAGAIN && errno != EWOULDBLOCK)
            return NULL;
        /* Wait till new connection is available. */
        int rc = fdwait(l->fd, FDW_IN, NULL);
        assert(rc == FDW_IN);
    }
}

tcpsock tcpconnect(const char *addr) {
    struct sockaddr_in addr_in;
    int rc = resolve_ip4_literal_addr(addr, &addr_in);
    if (rc != 0)
        return NULL;

    /* Open a socket. */
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if(s == -1)
        return NULL;
    int opt = 1;
    rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
    assert(rc == 0);

    /* Make it non-blocking. */
    opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    assert(rc != -1);

    /* Connect to the remote endpoint. */
    rc = connect(s, (struct sockaddr*)&addr_in, sizeof(addr_in));
    if(rc != 0) {
        assert(rc == -1);
        if(errno != EINPROGRESS)
            return NULL;
        rc = fdwait(s, FDW_OUT, NULL);
        assert(rc == FDW_OUT);
        int err;
        socklen_t errsz = sizeof(err);
        rc = getsockopt(s, SOL_SOCKET, SO_ERROR, (void*)&err, &errsz);
        if(rc != 0) {
            err = errno;
            close(s);
            errno = err;
            return NULL;
        }
        if(err != 0) {
            close(s);
            errno = err;
            return NULL;
        }
    }

    /* Create the object. */
    return &tcpconn_create(s)->sock;
}

void tcpsend(tcpsock s, const void *buf, size_t len) {
    if(s->type != MILL_TCPCONN)
        mill_panic("trying to send to an unconnected socket");
    struct tcpconn *conn = (struct tcpconn*)s;
    /* If there was already an error don't even try again. */
    if(conn->oerr)
        return;

    /* If it fits into the output buffer copy it there and be done. */
    if(conn->olen + len <= MILL_TCP_BUFLEN) {
        memcpy(&conn->obuf[conn->olen], buf, len);
        conn->olen += len;
        return;
    }

    /* Flush the output buffer. */
    tcpflush(s);

    /* Try to fit it into the buffer once again. */
    if(conn->olen + len <= MILL_TCP_BUFLEN) {
        memcpy(&conn->obuf[conn->olen], buf, len);
        conn->olen += len;
        return;
    }

    /* The data chunk to send is longer than the output buffer.
       Let's do the sending in-place. */
    char *pos = (char*)buf;
    size_t remaining = len;
    while(remaining) {
        ssize_t sz = send(conn->fd, pos, remaining, 0);
        if(sz == -1) {
            if(errno != EAGAIN && errno != EWOULDBLOCK) {
                conn->oerr = errno;
                return;
            }
            int rc = fdwait(conn->fd, FDW_OUT, NULL);
            assert(rc == FDW_OUT);
            continue;
        }
        pos += sz;
        remaining -= sz;
    }
}

int tcpflush(tcpsock s) {
    if(s->type != MILL_TCPCONN)
        mill_panic("trying to send to an unconnected socket");
    struct tcpconn *conn = (struct tcpconn*)s;
    if(conn->oerr) {
        errno = conn->oerr;
        conn->oerr = 0;
        conn->olen = 0;
        return -1;
    }
    if(!conn->olen)
        return 0;
    char *pos = conn->obuf;
    size_t remaining = conn->olen;
    while(remaining) {
        ssize_t sz = send(conn->fd, pos, remaining, 0);
        if(sz == -1) {
            if(errno != EAGAIN && errno != EWOULDBLOCK)
                return -1;
            int rc = fdwait(conn->fd, FDW_OUT, NULL);
            assert(rc == FDW_OUT);
            continue;
        }
        pos += sz;
        remaining -= sz;
    }
    conn->olen = 0;
    return 0;
}

ssize_t tcprecv(tcpsock s, void *buf, size_t len) {
    if(s->type != MILL_TCPCONN)
        mill_panic("trying to receive from an unconnected socket");
    struct tcpconn *conn = (struct tcpconn*)s;
    /* If there's enough data in the buffer it's easy. */
    if(conn->ilen >= len) {
        memcpy(buf, &conn->ibuf[conn->ifirst], len);
        conn->ifirst += len;
        conn->ilen -= len;
        return 0;
    }

    /* Let's move all the data from the buffer first. */
    char *pos = (char*)buf;
    size_t remaining = len;
    memcpy(pos, &conn->ibuf[conn->ifirst], conn->ilen);
    pos += conn->ilen;
    remaining -= conn->ilen;
    conn->ifirst = 0;
    conn->ilen = 0;

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
            ssize_t sz = recv(conn->fd, conn->ibuf, MILL_TCP_BUFLEN, 0);
            assert(sz != 0); // TODO
            if(sz == -1) {
                assert(errno == EAGAIN && errno == EWOULDBLOCK); // TODO
                sz = 0;
            }
            if(sz < remaining) {
                memcpy(pos, conn->ibuf, sz);
                pos += sz;
                remaining -= sz;
                conn->ifirst = 0;
                conn->ilen = 0;
            }
            else {
                memcpy(pos, conn->ibuf, remaining);
                conn->ifirst = remaining;
                conn->ilen = sz - remaining;
                return 0;
            }
        }

        /* Wait till there's more data to read. */
        int res = fdwait(conn->fd, FDW_IN, NULL);
        assert(res & FDW_IN);
    }
}

ssize_t tcprecvuntil(tcpsock s, void *buf, size_t len, char until) {
    if(s->type != MILL_TCPCONN)
        mill_panic("trying to receive from an unconnected socket");
    char *pos = (char*)buf;
    size_t i;
    for(i = 0; i != len; ++i, ++pos) {
        ssize_t res = tcprecv(s, pos, 1);
        if (res != 0)
            return -1;
        if(*pos == until)
            return i + 1;
    }
    return 0;
}

void tcpclose(tcpsock s) {
    if(s->type == MILL_TCPLISTENER) {
        struct tcplistener *l = (struct tcplistener*)s;
        int rc = close(l->fd);
        assert(rc == 0);
        free(l);
        return;
    }
    if(s->type == MILL_TCPCONN) {
        struct tcpconn *c = (struct tcpconn*)s;
        int rc = close(c->fd);
        assert(rc == 0);
        free(c);
        return;
    }
    assert(0);
}


