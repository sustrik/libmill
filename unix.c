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

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "debug.h"
#include "libmill.h"
#include "utils.h"

#ifndef MILL_UNIX_BUFLEN
#define MILL_UNIX_BUFLEN (4096)
#endif

enum mill_unixtype {
   MILL_UNIXLISTENER,
   MILL_UNIXCONN
};

struct mill_unixsock_ {
    enum mill_unixtype type;
};

struct mill_unixlistener {
    struct mill_unixsock_ sock;
    int fd;
};

struct mill_unixconn {
    struct mill_unixsock_ sock;
    int fd;
    size_t ifirst;
    size_t ilen;
    size_t olen;
    char ibuf[MILL_UNIX_BUFLEN];
    char obuf[MILL_UNIX_BUFLEN];
};

static void mill_unixtune(int s) {
    /* Make the socket non-blocking. */
    int opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    int rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    mill_assert(rc != -1);
    /* If possible, prevent SIGPIPE signal when writing to the connection
        already closed by the peer. */
#ifdef SO_NOSIGPIPE
    opt = 1;
    rc = setsockopt (s, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof (opt));
    mill_assert (rc == 0 || errno == EINVAL);
#endif
}

static int mill_unixresolve(const char *addr, struct sockaddr_un *su) {
    mill_assert(su);
    if (strlen(addr) >= sizeof(su->sun_path)) {
        errno = EINVAL;
        return -1;
    }
    su->sun_family = AF_UNIX;
    strncpy(su->sun_path, addr, sizeof(su->sun_path));
    errno = 0;
    return 0;
}

static void unixconn_init(struct mill_unixconn *conn, int fd) {
    conn->sock.type = MILL_UNIXCONN;
    conn->fd = fd;
    conn->ifirst = 0;
    conn->ilen = 0;
    conn->olen = 0;
}

struct mill_unixsock_ *mill_unixlisten_(const char *addr, int backlog) {
    struct sockaddr_un su;
    int rc = mill_unixresolve(addr, &su);
    if (rc != 0) {
        return NULL;
    }
    /* Open the listening socket. */
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if(s == -1)
        return NULL;
    mill_unixtune(s);

    /* Start listening. */
    rc = bind(s, (struct sockaddr*)&su, sizeof(struct sockaddr_un));
    if(rc != 0)
        return NULL;
    rc = listen(s, backlog);
    if(rc != 0)
        return NULL;

    /* Create the object. */
    struct mill_unixlistener *l = malloc(sizeof(struct mill_unixlistener));
    if(!l) {
        fdclean(s);
        close(s);
        errno = ENOMEM;
        return NULL;
    }
    l->sock.type = MILL_UNIXLISTENER;
    l->fd = s;
    errno = 0;
    return &l->sock;
}

struct mill_unixsock_ *mill_unixaccept_(struct mill_unixsock_ *s,
      int64_t deadline) {
    if(s->type != MILL_UNIXLISTENER)
        mill_panic("trying to accept on a socket that isn't listening");
    struct mill_unixlistener *l = (struct mill_unixlistener*)s;
    while(1) {
        /* Try to get new connection (non-blocking). */
        int as = accept(l->fd, NULL, NULL);
        if (as >= 0) {
            mill_unixtune(as);
            struct mill_unixconn *conn = malloc(sizeof(struct mill_unixconn));
            if(!conn) {
                fdclean(as);
                close(as);
                errno = ENOMEM;
                return NULL;
            }
            unixconn_init(conn, as);
            errno = 0;
            return (struct mill_unixsock_*)conn;
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

struct mill_unixsock_ *mill_unixconnect_(const char *addr) {
    struct sockaddr_un su;
    int rc = mill_unixresolve(addr, &su);
    if (rc != 0) {
        return NULL;
    }

    /* Open a socket. */
    int s = socket(AF_UNIX,  SOCK_STREAM, 0);
    if(s == -1)
        return NULL;
    mill_unixtune(s);

    /* Connect to the remote endpoint. */
    rc = connect(s, (struct sockaddr*)&su, sizeof(struct sockaddr_un));
    if(rc != 0) {
        int err = errno;
        mill_assert(rc == -1);
        fdclean(s);
        close(s);
        errno = err;
        return NULL;
    }

    /* Create the object. */
    struct mill_unixconn *conn = malloc(sizeof(struct mill_unixconn));
    if(!conn) {
        fdclean(s);
        close(s);
        errno = ENOMEM;
        return NULL;
    }
    unixconn_init(conn, s);
    errno = 0;
    return (struct mill_unixsock_*)conn;
}

void mill_unixpair_(struct mill_unixsock_ **a, struct mill_unixsock_ **b) {
    if(!a || !b) {
        errno = EINVAL;
        return;
    }
    int fd[2];
    int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
    if (rc != 0)
        return;
    mill_unixtune(fd[0]);
    mill_unixtune(fd[1]);
    struct mill_unixconn *conn = malloc(sizeof(struct mill_unixconn));
    if(!conn) {
        fdclean(fd[0]);
        close(fd[0]);
        fdclean(fd[1]);
        close(fd[1]);
        errno = ENOMEM;
        return;
    }
    unixconn_init(conn, fd[0]);
    *a = (struct mill_unixsock_*)conn;
    conn = malloc(sizeof(struct mill_unixconn));
    if(!conn) {
        free(*a);
        fdclean(fd[0]);
        close(fd[0]);
        fdclean(fd[1]);
        close(fd[1]);
        errno = ENOMEM;
        return;
    }
    unixconn_init(conn, fd[1]);
    *b = (struct mill_unixsock_*)conn;
    errno = 0;
}

size_t mill_unixsend_(struct mill_unixsock_ *s, const void *buf, size_t len,
      int64_t deadline) {
    if(s->type != MILL_UNIXCONN)
        mill_panic("trying to send to an unconnected socket");
    struct mill_unixconn *conn = (struct mill_unixconn*)s;

    /* If it fits into the output buffer copy it there and be done. */
    if(conn->olen + len <= MILL_UNIX_BUFLEN) {
        memcpy(&conn->obuf[conn->olen], buf, len);
        conn->olen += len;
        errno = 0;
        return len;
    }

    /* If it doesn't fit, flush the output buffer first. */
    unixflush(s, deadline);
    if(errno != 0)
        return 0;

    /* Try to fit it into the buffer once again. */
    if(conn->olen + len <= MILL_UNIX_BUFLEN) {
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

void mill_unixflush_(struct mill_unixsock_ *s, int64_t deadline) {
    if(s->type != MILL_UNIXCONN)
        mill_panic("trying to send to an unconnected socket");
    struct mill_unixconn *conn = (struct mill_unixconn*)s;
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

size_t mill_unixrecv_(struct mill_unixsock_ *s, void *buf, size_t len,
      int64_t deadline) {
    if(s->type != MILL_UNIXCONN)
        mill_panic("trying to receive from an unconnected socket");
    struct mill_unixconn *conn = (struct mill_unixconn*)s;
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
        if(remaining > MILL_UNIX_BUFLEN) {
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
            ssize_t sz = recv(conn->fd, conn->ibuf, MILL_UNIX_BUFLEN, 0);
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

size_t mill_unixrecvuntil_(struct mill_unixsock_ *s, void *buf, size_t len,
      const char *delims, size_t delimcount, int64_t deadline) {
    if(s->type != MILL_UNIXCONN)
        mill_panic("trying to receive from an unconnected socket");
    unsigned char *pos = (unsigned char*)buf;
    size_t i;
    for(i = 0; i != len; ++i, ++pos) {
        size_t res = unixrecv(s, pos, 1, deadline);
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

void mill_unixshutdown_(struct mill_unixsock_ *s, int how) {
    mill_assert(s->type == MILL_UNIXCONN);
    struct mill_unixconn *c = (struct mill_unixconn*)s;
    int rc = shutdown(c->fd, how);
    mill_assert(rc == 0 || errno == ENOTCONN);
}

void mill_unixclose_(struct mill_unixsock_ *s) {
    if(s->type == MILL_UNIXLISTENER) {
        struct mill_unixlistener *l = (struct mill_unixlistener*)s;
        fdclean(l->fd);
        int rc = close(l->fd);
        mill_assert(rc == 0);
        free(l);
        return;
    }
    if(s->type == MILL_UNIXCONN) {
        struct mill_unixconn *c = (struct mill_unixconn*)s;
        fdclean(c->fd);
        int rc = close(c->fd);
        mill_assert(rc == 0);
        free(c);
        return;
    }
    mill_assert(0);
}

