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

#include "debug.h"
#include "ip.h"
#include "libmill.h"
#include "utils.h"

/* The buffer size is based on typical Ethernet MTU (1500 bytes). Making it
   smaller would yield small suboptimal packets. Making it higher would bring
   no substantial benefit. The value is made smaller to account for IPv4/IPv6
   and TCP headers. Few more bytes are subtracted to account for any possible
   IP or TCP options */
#ifndef MILL_TCP_BUFLEN
#define MILL_TCP_BUFLEN (1500 - 68)
#endif

enum mill_tcptype {
   MILL_TCPLISTENER,
   MILL_TCPCONN
};

struct mill_tcpsock_ {
    enum mill_tcptype type;
};

struct mill_tcplistener {
    struct mill_tcpsock_ sock;
    int fd;
    int port;
};

struct mill_tcpconn {
    struct mill_tcpsock_ sock;
    int fd;
    size_t ifirst;
    size_t ilen;
    size_t olen;
    char ibuf[MILL_TCP_BUFLEN];
    char obuf[MILL_TCP_BUFLEN];
    ipaddr addr;
};

static void mill_tcptune(int s) {
    /* Make the socket non-blocking. */
    int opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    int rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    mill_assert(rc != -1);
    /*  Allow re-using the same local address rapidly. */
    opt = 1;
    rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
    mill_assert(rc == 0);
    /* If possible, prevent SIGPIPE signal when writing to the connection
        already closed by the peer. */
#ifdef SO_NOSIGPIPE
    opt = 1;
    rc = setsockopt (s, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof (opt));
    mill_assert (rc == 0 || errno == EINVAL);
#endif
}

static void tcpconn_init(struct mill_tcpconn *conn, int fd) {
    conn->sock.type = MILL_TCPCONN;
    conn->fd = fd;
    conn->ifirst = 0;
    conn->ilen = 0;
    conn->olen = 0;
}

struct mill_tcpsock_ *mill_tcplisten_(ipaddr addr, int backlog) {
    /* Open the listening socket. */
    int s = socket(mill_ipfamily(addr), SOCK_STREAM, 0);
    if(s == -1)
        return NULL;
    mill_tcptune(s);

    /* Start listening. */
    int rc = bind(s, (struct sockaddr*)&addr, mill_iplen(addr));
    if(rc != 0)
        return NULL;
    rc = listen(s, backlog);
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
    struct mill_tcplistener *l = malloc(sizeof(struct mill_tcplistener));
    if(!l) {
        fdclean(s);
        close(s);
        errno = ENOMEM;
        return NULL;
    }
    l->sock.type = MILL_TCPLISTENER;
    l->fd = s;
    l->port = port;
    errno = 0;
    return &l->sock;
}

int mill_tcpport_(struct mill_tcpsock_ *s) {
    if(s->type == MILL_TCPCONN) {
        struct mill_tcpconn *c = (struct mill_tcpconn*)s;
        return mill_ipport(c->addr);
    }
    else if(s->type == MILL_TCPLISTENER) {
        struct mill_tcplistener *l = (struct mill_tcplistener*)s;
        return l->port;
    }
    mill_assert(0);
}

struct mill_tcpsock_ *mill_tcpaccept_(struct mill_tcpsock_ *s, int64_t deadline) {
    if(s->type != MILL_TCPLISTENER)
        mill_panic("trying to accept on a socket that isn't listening");
    struct mill_tcplistener *l = (struct mill_tcplistener*)s;
    socklen_t addrlen;
    ipaddr addr;
    while(1) {
        /* Try to get new connection (non-blocking). */
        addrlen = sizeof(addr);
        int as = accept(l->fd, (struct sockaddr *)&addr, &addrlen);
        if (as >= 0) {
            mill_tcptune(as);
            struct mill_tcpconn *conn = malloc(sizeof(struct mill_tcpconn));
            if(!conn) {
                fdclean(as);
                close(as);
                errno = ENOMEM;
                return NULL;
            }
            tcpconn_init(conn, as);
            conn->addr = addr;
            errno = 0;
            return (tcpsock)conn;
        }
        mill_assert(as == -1);
        if(errno != EAGAIN && errno != EWOULDBLOCK)
            return NULL;
        /* Wait till new connection is available. */
        int rc = fdwait(l->fd, FDW_IN, deadline);
        if(rc == 0) {
            errno = ETIMEDOUT;
            return NULL;
        }
        if(rc & FDW_ERR)
            return NULL;
        mill_assert(rc == FDW_IN);
    }
}

struct mill_tcpsock_ *mill_tcpconnect_(ipaddr addr, int64_t deadline) {
    /* Open a socket. */
    int s = socket(mill_ipfamily(addr), SOCK_STREAM, 0);
    if(s == -1)
        return NULL;
    mill_tcptune(s);

    /* Connect to the remote endpoint. */
    int rc = connect(s, (struct sockaddr*)&addr, mill_iplen(addr));
    if(rc != 0) {
        mill_assert(rc == -1);
        if(errno != EINPROGRESS)
            return NULL;
        rc = fdwait(s, FDW_OUT, deadline);
        if(rc == 0) {
            errno = ETIMEDOUT;
            return NULL;
        }
        int err;
        socklen_t errsz = sizeof(err);
        rc = getsockopt(s, SOL_SOCKET, SO_ERROR, (void*)&err, &errsz);
        if(rc != 0) {
            err = errno;
            fdclean(s);
            close(s);
            errno = err;
            return NULL;
        }
        if(err != 0) {
            fdclean(s);
            close(s);
            errno = err;
            return NULL;
        }
    }

    /* Create the object. */
    struct mill_tcpconn *conn = malloc(sizeof(struct mill_tcpconn));
    if(!conn) {
        fdclean(s);
        close(s);
        errno = ENOMEM;
        return NULL;
    }
    tcpconn_init(conn, s);
    errno = 0;
    return (tcpsock)conn;
}

size_t mill_tcpsend_(struct mill_tcpsock_ *s, const void *buf, size_t len,
      int64_t deadline) {
    if(s->type != MILL_TCPCONN)
        mill_panic("trying to send to an unconnected socket");
    struct mill_tcpconn *conn = (struct mill_tcpconn*)s;

    /* If it fits into the output buffer copy it there and be done. */
    if(conn->olen + len <= MILL_TCP_BUFLEN) {
        memcpy(&conn->obuf[conn->olen], buf, len);
        conn->olen += len;
        errno = 0;
        return len;
    }

    /* If it doesn't fit, flush the output buffer first. */
    tcpflush(s, deadline);
    if(errno != 0)
        return 0;

    /* Try to fit it into the buffer once again. */
    if(conn->olen + len <= MILL_TCP_BUFLEN) {
        memcpy(&conn->obuf[conn->olen], buf, len);
        conn->olen += len;
        errno = 0;
        return len;
    }

    /* The data chunk to send is longer than the output buffer.
       Let's do the sending in-place. */
    char *pos = (char*)buf;
    size_t remaining = len;
    while(remaining) {
        ssize_t sz = send(conn->fd, pos, remaining, 0);
        if(sz == -1) {
            /* Operating systems are inconsistent w.r.t. returning EPIPE and
               ECONNRESET. Let's paper over it like this. */
            if(errno == EPIPE) {
                errno = ECONNRESET;
                return 0;
            }
            if(errno != EAGAIN && errno != EWOULDBLOCK)
                return 0;
            int rc = fdwait(conn->fd, FDW_OUT, deadline);
            if(rc == 0) {
                errno = ETIMEDOUT;
                return len - remaining;
            }
            continue;
        }
        pos += sz;
        remaining -= sz;
    }
    errno = 0;
    return len;
}

void mill_tcpflush_(struct mill_tcpsock_ *s, int64_t deadline) {
    if(s->type != MILL_TCPCONN)
        mill_panic("trying to send to an unconnected socket");
    struct mill_tcpconn *conn = (struct mill_tcpconn*)s;
    if(!conn->olen) {
        errno = 0;
        return;
    }
    char *pos = conn->obuf;
    size_t remaining = conn->olen;
    while(remaining) {
        ssize_t sz = send(conn->fd, pos, remaining, 0);
        if(sz == -1) {
            /* Operating systems are inconsistent w.r.t. returning EPIPE and
               ECONNRESET. Let's paper over it like this. */
            if(errno == EPIPE) {
                errno = ECONNRESET;
                return;
            }
            if(errno != EAGAIN && errno != EWOULDBLOCK)
                return;
            int rc = fdwait(conn->fd, FDW_OUT, deadline);
            if(rc == 0) {
                errno = ETIMEDOUT;
                return;
            }
            continue;
        }
        pos += sz;
        remaining -= sz;
    }
    conn->olen = 0;
    errno = 0;
}

size_t mill_tcprecv_(struct mill_tcpsock_ *s, void *buf, size_t len,
      int64_t deadline) {
    if(s->type != MILL_TCPCONN)
        mill_panic("trying to receive from an unconnected socket");
    struct mill_tcpconn *conn = (struct mill_tcpconn*)s;
    /* If there's enough data in the buffer it's easy. */
    if(conn->ilen >= len) {
        memcpy(buf, &conn->ibuf[conn->ifirst], len);
        conn->ifirst += len;
        conn->ilen -= len;
        errno = 0;
        return len;
    }

    /* Let's move all the data from the buffer first. */
    char *pos = (char*)buf;
    size_t remaining = len;
    memcpy(pos, &conn->ibuf[conn->ifirst], conn->ilen);
    pos += conn->ilen;
    remaining -= conn->ilen;
    conn->ifirst = 0;
    conn->ilen = 0;

    mill_assert(remaining);
    while(1) {
        if(remaining > MILL_TCP_BUFLEN) {
            /* If we still have a lot to read try to read it in one go directly
               into the destination buffer. */
            ssize_t sz = recv(conn->fd, pos, remaining, 0);
            if(!sz) {
		        errno = ECONNRESET;
		        return len - remaining;
            }
            if(sz == -1) {
                if(errno != EAGAIN && errno != EWOULDBLOCK)
                    return len - remaining;
                sz = 0;
            }
            if((size_t)sz == remaining) {
                errno = 0;
                return len;
            }
            pos += sz;
            remaining -= sz;
        }
        else {
            /* If we have just a little to read try to read the full connection
               buffer to minimise the number of system calls. */
            ssize_t sz = recv(conn->fd, conn->ibuf, MILL_TCP_BUFLEN, 0);
            if(!sz) {
		        errno = ECONNRESET;
		        return len - remaining;
            }
            if(sz == -1) {
                if(errno != EAGAIN && errno != EWOULDBLOCK)
                    return len - remaining;
                sz = 0;
            }
            if((size_t)sz < remaining) {
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
                errno = 0;
                return len;
            }
        }

        /* Wait till there's more data to read. */
        int res = fdwait(conn->fd, FDW_IN, deadline);
        if(!res) {
            errno = ETIMEDOUT;
            return len - remaining;
        }
    }
}

size_t mill_tcprecvuntil_(struct mill_tcpsock_ *s, void *buf, size_t len,
      const char *delims, size_t delimcount, int64_t deadline) {
    if(s->type != MILL_TCPCONN)
        mill_panic("trying to receive from an unconnected socket");
    char *pos = (char*)buf;
    size_t i;
    for(i = 0; i != len; ++i, ++pos) {
        size_t res = tcprecv(s, pos, 1, deadline);
        if(res == 1) {
            size_t j;
            for(j = 0; j != delimcount; ++j)
                if(*pos == delims[j])
                    return i + 1;
        }
        if (errno != 0)
            return i + res;
    }
    errno = ENOBUFS;
    return len;
}

void mill_tcpshutdown_(struct mill_tcpsock_ *s, int how) {
    mill_assert(s->type == MILL_TCPCONN);
    struct mill_tcpconn *c = (struct mill_tcpconn*)s;
    int rc = shutdown(c->fd, how);
    mill_assert(rc == 0 || errno == ENOTCONN);
}

void mill_tcpclose_(struct mill_tcpsock_ *s) {
    if(s->type == MILL_TCPLISTENER) {
        struct mill_tcplistener *l = (struct mill_tcplistener*)s;
        fdclean(l->fd);
        int rc = close(l->fd);
        mill_assert(rc == 0);
        free(l);
        return;
    }
    if(s->type == MILL_TCPCONN) {
        struct mill_tcpconn *c = (struct mill_tcpconn*)s;
        fdclean(c->fd);
        int rc = close(c->fd);
        mill_assert(rc == 0);
        free(c);
        return;
    }
    mill_assert(0);
}

ipaddr mill_tcpaddr_(struct mill_tcpsock_ *s) {
    if(s->type != MILL_TCPCONN)
        mill_panic("trying to get address from a socket that isn't connected");
    struct mill_tcpconn *l = (struct mill_tcpconn *)s;
    return l->addr;
}

/* This function is to be used only internally by libmill. Take into account
   that once there are data in tcpsock's tx/rx buffers, the state of fd may
   not match the state of tcpsock object. Works only on connected sockets. */
int mill_tcpfd(struct mill_tcpsock_ *s) {
    return ((struct mill_tcpconn*)s)->fd;
}

