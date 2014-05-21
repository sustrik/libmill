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

#define mill_cont(ptr, type, member) \
    (ptr ? ((type*) (((char*) ptr) - offsetof(type, member))) : NULL)

struct mill_coframe_head;
struct mill_loop;

typedef void (*mill_handler_fn) (
    struct mill_coframe_head *self,
    struct mill_coframe_head* ev);

struct mill_coframe_head {
    mill_handler_fn handler; 
    int state;
    int tag;
    int err;
    struct mill_coframe_head *parent;
    struct mill_coframe_head *next;
    struct mill_loop *loop;
};

void mill_coframe_head_init (
    struct mill_coframe_head *self,
    mill_handler_fn handler,
    struct mill_coframe_head *parent,
    struct mill_loop *loop,
    int tag);

void mill_coframe_head_term (struct mill_coframe_head *self);

void mill_coframe_head_emit (struct mill_coframe_head *self, int err);

#define mill_wait(statearg)\
    do {\
        cf->mill_cfh.state = (statearg);\
        return;\
        state##statearg:\
        ;\
    } while (0)

#define mill_return(errarg)\
    do {\
        cf->mill_cfh.err = (arrarg);\
        mill_coframe_head_emit (&cf->mill_cfh);\
        return;\
    } while (0)

/******************************************************************************/
/* The event loop.                                                            */
/******************************************************************************/

struct mill_loop
{
    uv_loop_t uv_loop;
    uv_idle_t idle;

    /* Local event queue. Items in this list are processed immediately,
       before control is returned to libuv. */
    struct mill_coframe_head *first;
    struct mill_coframe_head *last;
};

void mill_loop_init (struct mill_loop *self);
void mill_loop_term (struct mill_loop *self);
void mill_loop_run (struct mill_loop *self);
void mill_loop_emit (struct mill_loop *self, struct mill_coframe_head *base);

/******************************************************************************/
/* Alarm.                                                                     */
/******************************************************************************/

/* Coframe for coroutine alarm. */
struct mill_coframe_alarm {

    /* Generic coframe header. */
    struct mill_coframe_head mill_cfh;

    /* Local variables. */
    uv_timer_t timer;
};

void mill_call_alarm (
    struct mill_coframe_alarm *cf,
    struct mill_loop *loop,
    struct mill_coframe_head *parent,    
    int tag,
    int milliseconds);

/******************************************************************************/
/* TCP socket.                                                                */
/******************************************************************************/

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

