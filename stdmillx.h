/*
    Copyright (c) 2014 Martin Sustrik  All rights reserved.

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

/* This header is used inernally by mill. Don't use it directly in your code! */

#ifndef stdmillx_h_included
#define stdmillx_h_included

struct tcpsocket {
    uv_tcp_t s;
    uv_loop_t *loop;
    int pc;

    /* Handle of the receive coroutine currently being executed on this socket.
       This includes any socket-scoped asynchronous operations such as connect,
       accept or term. */
    void *recvcfptr;

    /* Handle of the send coroutine currently being executed on this socket. */
    void *sendcfptr;
};

int tcpsocket_init (
    struct tcpsocket *self,
    struct mill_loop *loop);

int tcpsocket_bind (
    struct tcpsocket *self,
    struct sockaddr *addr,
    int flags);

int tcpsocket_listen (
    struct tcpsocket *self,
    int backlog);

#endif
