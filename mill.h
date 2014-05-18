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

#ifndef MILL_H_INCLUDED
#define MILL_H_INCLUDED

#include <assert.h>
#include <stdlib.h>
#include <uv.h>

/******************************************************************************/
/* Generic stuff.                                                             */
/******************************************************************************/

struct mill_cfhead;
struct mill_loop;

typedef void (*mill_handler_fn) (
    struct mill_cfhead *self,
    struct mill_cfhead* ev);

struct mill_cfhead {
    mill_handler_fn handler; 
    int state;
    int tag;
    int err;
    struct mill_cfhead *parent;
    struct mill_cfhead *next;
    struct mill_loop *loop;
};

void mill_cfhead_init (
    struct mill_cfhead *self,
    mill_handler_fn handler,
    struct mill_cfhead *parent,
    struct mill_loop *loop,
    int tag);

void mill_cfhead_term (struct mill_cfhead *self);

void mill_cfhead_emit (struct mill_cfhead *self, int err);

#define mill_wait(statearg)\
    do {\
        self->mill_cfhead.state = (statearg);\
        return;\
        state##statearg:\
        ;\
    } while (0)

#define mill_return(errarg)\
    do {\
        self->mill_cfhead.err = (arrarg);\
        mill_cfhead_emit (&self->mill_cfhead);\
        return;\
    } while (0)

/******************************************************************************/
/* The event loop.                                                            */
/******************************************************************************/

struct mill_loop
{
    uv_loop_t uv_loop;

    /* Local event queue. Items in this list are processed immediately,
       before control is returned to libuv. */
    struct mill_cfhead *first;
    struct mill_cfhead *last;
};

void mill_loop_init (struct mill_loop *self);
void mill_loop_term (struct mill_loop *self);
void mill_loop_run (struct mill_loop *self);
void mill_loop_emit (struct mill_loop *self, struct mill_cfhead *base);

/******************************************************************************/
/* Alarm.                                                                     */
/******************************************************************************/

struct mill_coroutine_alarm {
    struct mill_cfhead mill_cfhead;
    uv_timer_t timer;
};

void mill_call_alarm (
    struct mill_coroutine_alarm *cf,
    int milliseconds,
    struct mill_cfhead *parent,
    struct mill_loop *loop,
    int tag);

/******************************************************************************/
/* TCP socket.                                                                */
/******************************************************************************/

struct tcpsocket {
    uv_tcp_t s;
    uv_loop_t *loop;
    int state;
    struct mill_cfhead *recvop;
    struct mill_cfhead *sendop;
};

int tcpsocket_init (
    struct tcpsocket *self,
    struct mill_loop *loop);

/*
    coroutine tcpsocket_term (
        struct tcpsocket *self);
*/

struct mill_coroutine_tcpsocket_term {
    struct mill_cfhead mill_cfhead;
};

void mill_call_tcpsocket_term (
    struct mill_coroutine_tcpsocket_term *cf,
    struct tcpsocket *self,
    struct mill_cfhead *parent,
    struct mill_loop *loop,
    int tag);

int tcpsocket_bind (
    struct tcpsocket *self,
    struct sockaddr *addr,
    int flags);

/*
    coroutine tcpsocket_connect (
        struct tcpsocket *self,
        struct sockaddr *addr);
*/

struct mill_coroutine_tcpsocket_connect {
    struct mill_cfhead mill_cfhead;
    struct tcpsocket *self;
    uv_connect_t req;
};

void mill_call_tcpsocket_connect (
    struct mill_coroutine_tcpsocket_connect *cf,
    struct tcpsocket *self,
    struct sockaddr *addr,
    struct mill_cfhead *parent,
    struct mill_loop *loop,
    int tag);

int tcpsocket_listen (
    struct tcpsocket *self,
    int backlog,
    struct mill_loop *loop);

/*
    coroutine tcpsocket_accept (
        struct tcpsocket *self,
        struct tcpsocket *newsock);
*/

struct mill_coroutine_tcpsocket_accept {
    struct mill_cfhead mill_cfhead;
    struct tcpsocket *newsock;
};

void mill_call_tcpsocket_accept (
    struct mill_coroutine_tcpsocket_accept *cf,
    struct tcpsocket *self,
    struct tcpsocket *newsock,
    struct mill_cfhead *parent,
    struct mill_loop *loop,
    int tag);

/*
    coroutine tcpsocket_send (
        struct tcpsocket *self,
        const void *buf,
        size_t len);
*/

struct mill_coroutine_tcpsocket_send {
    struct mill_cfhead mill_cfhead;
    uv_write_t req;
    uv_buf_t buf;
};

void mill_call_tcpsocket_send (
    struct mill_coroutine_tcpsocket_send *cf,
    struct tcpsocket *self,
    const void *buf,
    size_t len,
    struct mill_cfhead *parent,
    struct mill_loop *loop,
    int tag);

#endif

