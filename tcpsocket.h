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

#ifndef TCPSOCKET_H_INCLUDED
#define TCPSOCKET_H_INCLUDED

#include "mill.h"

struct tcpsocket {
    uv_tcp_t s;
    uv_loop_t *loop;
    int state;
    struct mill_coframe_head *recvop;
    struct mill_coframe_head *sendop;
};

int tcpsocket_init (
    struct tcpsocket *self,
    struct mill_loop *loop);

/*
    coroutine tcpsocket_term (
        struct tcpsocket *self);
*/

struct mill_coframe_tcpsocket_term {

    /* Generic coframe header. */
    struct mill_coframe_head mill_cfh;

    /* Coroutine arguments. */
    struct tcpsocket *self;
};

void *mill_call_tcpsocket_term (
    struct mill_coframe_tcpsocket_term *cf,
    struct mill_loop *loop,
    struct mill_coframe_head *parent,
    int tag,
    struct tcpsocket *self);

int tcpsocket_bind (
    struct tcpsocket *self,
    struct sockaddr *addr,
    int flags);

/*
    coroutine tcpsocket_connect (
        struct tcpsocket *self,
        struct sockaddr *addr);
*/

struct mill_coframe_tcpsocket_connect {

    /* Generic coframe header. */
    struct mill_coframe_head mill_cfh;

    /* Coroutine arguments. */
    struct tcpsocket *self;

    /* Local variables. */
    uv_connect_t req;
};

void *mill_call_tcpsocket_connect (
    struct mill_coframe_tcpsocket_connect *cf,
    struct mill_loop *loop,
    struct mill_coframe_head *parent,
    int tag,
    struct tcpsocket *self,
    struct sockaddr *addr);

int tcpsocket_listen (
    struct tcpsocket *self,
    int backlog,
    struct mill_loop *loop);

/*
    coroutine tcpsocket_accept (
        struct tcpsocket *self,
        struct tcpsocket *newsock);
*/

struct mill_coframe_tcpsocket_accept {

    /* Generic coframe header. */
    struct mill_coframe_head mill_cfh;

    /* Coroutine arguments. */
    struct tcpsocket *self;
    struct tcpsocket *newsock;
};

void *mill_call_tcpsocket_accept (
    struct mill_coframe_tcpsocket_accept *cf,
    struct mill_loop *loop,
    struct mill_coframe_head *parent,
    int tag,
    struct tcpsocket *self,
    struct tcpsocket *newsock);

/*
    coroutine tcpsocket_send (
        struct tcpsocket *self,
        const void *buf,
        size_t len);
*/

struct mill_coframe_tcpsocket_send {

    /* Generic coframe header. */
    struct mill_coframe_head mill_cfh;

    /* Coroutine arguments. */
    struct tcpsocket *self;

    /* Local variables. */
    uv_write_t req;
    uv_buf_t buffer;
};

void *mill_call_tcpsocket_send (
    struct mill_coframe_tcpsocket_send *cf,
    struct mill_loop *loop,
    struct mill_coframe_head *parent,
    int tag,
    struct tcpsocket *self,
    const void *buf,
    size_t len);

/*
    coroutine tcpsocket_recv (
        struct tcpsocket *self,
        void *buf,
        size_t len);
*/

struct mill_coframe_tcpsocket_recv {

    /* Generic coframe header. */
    struct mill_coframe_head mill_cfh;

    /* Coroutine arguments. */
    struct tcpsocket *self;

    /* Local variables. */
    void *buf;
    size_t len;
};

void *mill_call_tcpsocket_recv (
    struct mill_coframe_tcpsocket_recv *cf,
    struct mill_loop *loop,
    struct mill_coframe_head *parent,
    int tag,
    struct tcpsocket *self,
    void *buf,
    size_t len);

#endif

