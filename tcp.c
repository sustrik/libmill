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
	
static void mill_tunesock(int s) {
    /* Make the socket non-blocking. */
    int opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    int rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    assert(rc != -1);
    /*  Allow re-using the same local address rapidly. */
    opt = 1;
    rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
    assert(rc == 0);
    /* If possible, prevent SIGPIPE signal when writing to the connection
        already closed by the peer. */
#ifdef SO_NOSIGPIPE
    opt = 1;
    rc = setsockopt (s, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof (opt));
    assert (rc == 0 || errno == EINVAL);
#endif
}

enum mill_tcptype {
   MILL_TCPLISTENER,
   MILL_TCPCONN
};

struct mill_tcpsock {
    enum mill_tcptype type;
};

struct mill_tcplistener {
    struct mill_tcpsock sock;
    int fd;
    int port;
};

struct mill_tcpconn {
    struct mill_tcpsock sock;
    int fd;
    int ifirst;
    int ilen;
    int olen;
    char ibuf[MILL_TCP_BUFLEN];
    char obuf[MILL_TCP_BUFLEN];
};

static struct mill_tcpconn *tcpconn_create(int fd) {
    struct mill_tcpconn *conn = malloc(sizeof(struct mill_tcpconn));
    assert(conn);
    conn->sock.type = MILL_TCPCONN;
    conn->fd = fd;
    conn->ifirst = 0;
    conn->ilen = 0;
    conn->olen = 0;
    return conn;
}

/* Convert textual IPv4 or IPv6 address to a binary one. */
static int mill_tcpresolve(const char *addr, int port,
      struct sockaddr_storage *ss, socklen_t *len) {
    assert(ss);
    if(port < 0 || port > 0xffff) {
        errno = EINVAL;
        return -1;
    }
    // NULL translates to INADDR_ANY.
    // TODO: Consider using in6addr_any. AFAICS that should account for IPv4
    //       addresses as well. However, would it work on all systems?
    if(!addr) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in*)ss;
        ipv4->sin_family = AF_INET;
        ipv4->sin_addr.s_addr = INADDR_ANY;
        ipv4->sin_port = htons((uint16_t)port);
        if(len)
            *len = sizeof(struct sockaddr_in);
        errno = 0;
        return 0;
    }
    // Try to interpret the string is IPv4 address.
    int rc = inet_pton(AF_INET, addr, ss);
    assert(rc >= 0);
    if(rc == 1) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in*)ss;
        ipv4->sin_family = AF_INET;
        ipv4->sin_port = htons((uint16_t)port);
        if(len)
            *len = sizeof(struct sockaddr_in);
        errno = 0;
        return 0;
    }

    // It's not an IPv4 address. Let's try to interpret it as IPv6 address.
    rc = inet_pton(AF_INET6, addr, ss);
    assert(rc >= 0);
    if(rc == 1) {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)ss;
        ipv6->sin6_family = AF_INET6;
        ipv6->sin6_port = htons((uint16_t)port);
        if(len)
            *len = sizeof(struct sockaddr_in6);
        errno = 0;
        return 0;
    }
    
    // It's neither IPv4, nor IPv6 address.
    errno = EINVAL;
    return -1;
}

tcpsock tcplisten(const char *addr, int port) {
    struct sockaddr_storage ss;
    socklen_t len;
    int rc = mill_tcpresolve(addr, port, &ss, &len);
    if (rc != 0)
        return NULL;

    /* Open the listening socket. */
    int s = socket(ss.ss_family, SOCK_STREAM, 0);
    if(s == -1)
        return NULL;
    mill_tunesock(s);

    /* Start listening. */
    rc = bind(s, (struct sockaddr*)&ss, len);
    assert(rc != -1);
    rc = listen(s, MILL_TCP_LISTEN_BACKLOG);
    assert(rc != -1);

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
            assert(0);
    }

    /* Create the object. */
    struct mill_tcplistener *l = malloc(sizeof(struct mill_tcplistener));
    assert(l);
    l->sock.type = MILL_TCPLISTENER;
    l->fd = s;
    l->port = port;
    errno = 0;
    return &l->sock;
}

int tcpport(tcpsock s) {
    if(s->type != MILL_TCPLISTENER)
        mill_panic("trying to get port from a socket that isn't listening");
    struct mill_tcplistener *l = (struct mill_tcplistener*)s;
    return l->port;
}

int tcpfd(tcpsock s) {
    if (s->type == MILL_TCPLISTENER) {
      struct mill_tcplistener *l = (struct mill_tcplistener*)s;
      return l->fd;
    } else {
      struct mill_tcpconn *c = (struct mill_tcpconn*)s;
      return c->fd;
    }
}

tcpsock tcpaccept(tcpsock s, int64_t deadline) {
    if(s->type != MILL_TCPLISTENER)
        mill_panic("trying to accept on a socket that isn't listening");
    struct mill_tcplistener *l = (struct mill_tcplistener*)s;
    while(1) {
        /* Try to get new connection (non-blocking). */
        int as = accept(l->fd, NULL, NULL);
        if (as >= 0) {
            mill_tunesock(as);
            errno = 0;
            return &tcpconn_create(as)->sock;
        }
        assert(as == -1);
        if(errno != EAGAIN && errno != EWOULDBLOCK)
            return NULL;
        /* Wait till new connection is available. */
        int rc = fdwait(l->fd, FDW_IN, deadline);
        if(rc == 0) {
            errno = ETIMEDOUT;
            return NULL;
        }
        assert(rc == FDW_IN);
    }
}

tcpsock tcpconnect(const char *addr, int port, int64_t deadline) {
    struct sockaddr_storage ss;
    socklen_t len;
    int rc = mill_tcpresolve(addr, port, &ss, &len);
    if (rc != 0)
        return NULL;

    /* Open a socket. */
    int s = socket(ss.ss_family, SOCK_STREAM, 0);
    if(s == -1)
        return NULL;
    mill_tunesock(s);

    /* Connect to the remote endpoint. */
    rc = connect(s, (struct sockaddr*)&ss, len);
    if(rc != 0) {
        assert(rc == -1);
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
    errno = 0;
    return &tcpconn_create(s)->sock;
}

size_t tcpsend(tcpsock s, const void *buf, size_t len, int64_t deadline) {
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
            if(errno != EAGAIN && errno != EWOULDBLOCK)
                return 0;
            int rc = fdwait(conn->fd, FDW_OUT, deadline);
            if(rc == 0) {
                errno = ETIMEDOUT;
                return len - remaining;
            }
            assert(rc == FDW_OUT);
            continue;
        }
        pos += sz;
        remaining -= sz;
    }
}

void tcpflush(tcpsock s, int64_t deadline) {
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
            if(errno != EAGAIN && errno != EWOULDBLOCK)
                return;
            int rc = fdwait(conn->fd, FDW_OUT, deadline);
            if(rc == 0) {
                errno = ETIMEDOUT;
                return;
            }
            assert(rc == FDW_OUT);
            continue;
        }
        pos += sz;
        remaining -= sz;
    }
    conn->olen = 0;
    errno = 0;
}

size_t tcprecv(tcpsock s, void *buf, size_t len, int64_t deadline) {
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

    assert(remaining);
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
            if(sz == remaining) {
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
        assert(res & FDW_IN);
    }
}

size_t tcprecvuntil(tcpsock s, void *buf, size_t len, unsigned char until,
      int64_t deadline) {
    if(s->type != MILL_TCPCONN)
        mill_panic("trying to receive from an unconnected socket");
    unsigned char *pos = (unsigned char*)buf;
    size_t i;
    for(i = 0; i != len; ++i, ++pos) {
        size_t res = tcprecv(s, pos, 1, deadline);
        if(res == 1 && *pos == until)
            return i + 1;
        if (errno != 0)
            return i + res;
    }
    errno = ENOBUFS;
    return len;
}

void tcpclose(tcpsock s) {
    if(s->type == MILL_TCPLISTENER) {
        struct mill_tcplistener *l = (struct mill_tcplistener*)s;
        int rc = close(l->fd);
        assert(rc == 0);
        free(l);
        return;
    }
    if(s->type == MILL_TCPCONN) {
        struct mill_tcpconn *c = (struct mill_tcpconn*)s;
        int rc = close(c->fd);
        assert(rc == 0);
        free(c);
        return;
    }
    assert(0);
}

