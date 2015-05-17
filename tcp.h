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

#ifndef MILL_TCP_H_INCLUDED
#define MILL_TCP_H_INCLUDED

#include <sys/types.h>
#include <sys/socket.h>

#include "mill.h"

typedef struct tcplistener *tcplistener;
typedef struct tcpconn *tcpconn;

MILL_EXPORT tcplistener tcplisten(const struct sockaddr *addr,
    socklen_t addrlen);
MILL_EXPORT tcpconn tcpaccept(tcplistener listener);
MILL_EXPORT void tcplistener_close(tcplistener listener);
MILL_EXPORT tcpconn tcpconnect(const struct sockaddr *addr, socklen_t addrlen);
MILL_EXPORT void tcpconn_close(tcpconn conn);
MILL_EXPORT void tcpwrite(tcpconn conn, const void *buf, size_t len);
MILL_EXPORT int tcpflush(tcpconn conn);
MILL_EXPORT ssize_t tcpread(tcpconn conn, void *buf, size_t len);
MILL_EXPORT ssize_t tcpreaduntil(tcpconn conn, void *buf, size_t len,
    char until);

#endif
