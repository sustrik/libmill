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

struct mill_base;
struct mill_loop;

typedef void (*mill_handler_fn) (
    struct mill_base *self,
    struct mill_base* ev);

struct mill_base {
    mill_handler_fn handler; 
    int state;
    int tag;
    int err;
    struct mill_base *parent;
    struct mill_base *next;
    struct mill_loop *loop;
};

void mill_base_init (
    struct mill_base *self,
    mill_handler_fn handler,
    struct mill_base *parent,
    struct mill_loop *loop,
    int tag);

void mill_base_term (struct mill_base *self);

void mill_base_emit (struct mill_base *self, int err);

#define mill_wait(statearg)\
    do {\
        self->mill_base.state = (statearg);\
        return;\
        state##statearg:\
        ;\
    } while (0)

#define mill_return(errarg)\
    do {\
        self->mill_base.err = (arrarg);\
        mill_base_emit (&self->mill_base);\
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
    struct mill_base *first;
    struct mill_base *last;
};

void mill_loop_init (struct mill_loop *self);
void mill_loop_term (struct mill_loop *self);
void mill_loop_run (struct mill_loop *self);
void mill_loop_emit (struct mill_loop *self, struct mill_base *base);

/******************************************************************************/
/* Alarm.                                                                     */
/******************************************************************************/

struct mill_coroutine_alarm {
    struct mill_base mill_base;
    uv_timer_t timer;
};

void mill_call_alarm (
    struct mill_coroutine_alarm *self,
    int milliseconds,
    struct mill_base *parent,
    struct mill_loop *loop,
    int tag);

/******************************************************************************/
/* TCP socket.                                                                */
/******************************************************************************/

struct tcpsocket {
    uv_tcp_t s;
    uv_loop_t *loop;

    /*  When this socket is listening for new connection, this pointer
        points to the associated coroutine. */
    struct mill_coroutine_tcpsocket_accept *accept;

    /*  When this socket is being closed, this pointer points to the associated
        coroutine. */
    struct mill_coroutine_tcpsocket_term *term;
};

int tcpsocket_init (
    struct tcpsocket *self,
    struct mill_loop *loop);

/*
    coroutine tcpsocket_term (
        struct tcpsocket *s);
*/

struct mill_coroutine_tcpsocket_term {
    struct mill_base mill_base;
    struct tcpsocket *s;
};

void mill_call_tcpsocket_term (
    struct mill_coroutine_tcpsocket_term *self,
    struct tcpsocket *s,
    struct mill_base *parent,
    struct mill_loop *loop,
    int tag);

int tcpsocket_bind (
    struct tcpsocket *s,
    struct sockaddr *addr,
    int flags);

/*
    coroutine tcpsocket_connect (
        struct tcpsocket *s,
        struct sockaddr *addr);
*/

struct mill_coroutine_tcpsocket_connect {
    struct mill_base mill_base;
    uv_connect_t conn;
};

void mill_call_tcpsocket_connect (
    struct mill_coroutine_tcpsocket_connect *self,
    struct tcpsocket *s,
    struct sockaddr *addr,
    struct mill_base *parent,
    struct mill_loop *loop,
    int tag);

int tcpsocket_listen (
    struct tcpsocket *s,
    int backlog,
    struct mill_loop *loop);

/*
    coroutine tcpsocket_accept (
        struct tcpsocket *s,
        struct tcpsocket *newsock);
*/

struct mill_coroutine_tcpsocket_accept {
    struct mill_base mill_base;
    struct tcpsocket *s;
};

void mill_call_tcpsocket_accept (
    struct mill_coroutine_tcpsocket_accept *self,
    struct tcpsocket *ls,
    struct tcpsocket *s,
    struct mill_base *parent,
    struct mill_loop *loop,
    int tag);

/*
    coroutine tcpsocket_send (
        struct tcpsocket *s,
        const void *buf,
        size_t len);
*/

struct mill_coroutine_tcpsocket_send {
    struct mill_base mill_base;
    uv_write_t req;
    uv_buf_t buf;
};

void mill_call_tcpsocket_send (
    struct mill_coroutine_tcpsocket_send *self,
    struct tcpsocket *s,
    const void *buf,
    size_t len,
    struct mill_base *parent,
    struct mill_loop *loop,
    int tag);

#endif

