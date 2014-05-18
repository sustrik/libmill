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

#include "mill.h"

#include <stdio.h>

/******************************************************************************/
/* Generic stuff.                                                             */
/******************************************************************************/

#define mill_cont(ptr, type, member) \
    (ptr ? ((type*) (((char*) ptr) - offsetof(type, member))) : NULL)

void mill_cfhead_init (
    struct mill_cfhead *self,
    mill_handler_fn handler,
    struct mill_cfhead *parent,
    struct mill_loop *loop,
    int tag)
{
    self->handler = handler;
    self->state = 0;
    self->tag = tag;
    self->err = 0;
    self->parent = parent;
    self->next = 0;
    self->loop = loop;
}

void mill_cfhead_term (
    struct mill_cfhead *self)
{
    assert (0);
}

void mill_cfhead_emit (
    struct mill_cfhead *self,
    int err)
{
    self->err = err;
    mill_loop_emit (self->loop, self);
}

/******************************************************************************/
/* The event loop.                                                            */
/******************************************************************************/

static void loop_cb (uv_idle_t* handle)
{
    struct mill_loop *self;

    self = mill_cont (handle, struct mill_loop, idle);

    while (self->first) {
        if (!self->first->parent) {
            uv_stop (&self->uv_loop);
            return;
        }
        self->first->parent->handler (self->first->parent, self->first);
        self->first = self->first->next;
    }
    self->last = 0;
}

void mill_loop_init (
    struct mill_loop *self)
{
    int rc;

    rc = uv_loop_init (&self->uv_loop);
    assert (rc == 0);
    rc = uv_idle_init (&self->uv_loop, &self->idle);
    assert (rc == 0);
    rc = uv_idle_start (&self->idle, loop_cb);
    assert (rc == 0);
    self->first = 0;
    self->last = 0;
}

void mill_loop_term (
    struct mill_loop *self)
{
    assert (0);
}

void mill_loop_run (
    struct mill_loop *self)
{
    int rc;

    rc = uv_run (&self->uv_loop, UV_RUN_DEFAULT);
    assert (rc >= 0);
}

void mill_loop_emit (
    struct mill_loop *self,
    struct mill_cfhead *ev)
{
    if (self->first == 0)
        self->first = ev;
    else
        self->last->next = ev;
    ev->next = 0;
    self->last = ev;
}

/******************************************************************************/
/* Alarm.                                                                     */
/******************************************************************************/

static void alarm_handler(
    struct mill_cfhead *base,
    struct mill_cfhead *event)
{
    struct mill_coroutine_alarm *cf;

    cf = mill_cont (base, struct mill_coroutine_alarm, mill_cfhead);
    assert (0);
}

static void alarm_cb (
    uv_timer_t *timer)
{
    struct mill_coroutine_alarm *cf;

    cf = mill_cont (timer, struct mill_coroutine_alarm, timer);
    mill_cfhead_emit (&cf->mill_cfhead, 0);
}

void mill_call_alarm (
    struct mill_coroutine_alarm *cf,
    int milliseconds,
    struct mill_cfhead *parent,
    struct mill_loop *loop,
    int tag)
{
    mill_cfhead_init (&cf->mill_cfhead, alarm_handler, parent, loop, tag);
    uv_timer_init(&loop->uv_loop, &cf->timer);
    uv_timer_start(&cf->timer, alarm_cb, milliseconds, 0);
}

/******************************************************************************/
/* TCP socket.                                                                */
/******************************************************************************/

#define TCPSOCKET_STATE_INIT 1
#define TCPSOCKET_STATE_CONNECTING 2
#define TCPSOCKET_STATE_LISTENING 3
#define TCPSOCKET_STATE_ACCEPTING 4
#define TCPSOCKET_STATE_ACTIVE 5
#define TCPSOCKET_STATE_TERMINATING 6

int tcpsocket_init (
    struct tcpsocket *self,
    struct mill_loop *loop)
{
    self->loop = &loop->uv_loop;
    self->state = TCPSOCKET_STATE_INIT;
    self->recvop = 0;
    self->sendop = 0;
    return uv_tcp_init (&loop->uv_loop, &self->s);
}

static void tcpsocket_term_handler (
    struct mill_cfhead *base,
    struct mill_cfhead *event)
{
    struct mill_coroutine_tcpsocket_term *cf;

    cf = mill_cont(base, struct mill_coroutine_tcpsocket_term, mill_cfhead);
    assert (0);
}

static void tcpsocket_term_cb (
    uv_handle_t* handle)
{
    struct tcpsocket *self = mill_cont (handle, struct tcpsocket, s);

    assert (self->state == TCPSOCKET_STATE_TERMINATING);
    assert (self->recvop != 0);

    mill_cfhead_emit (self->recvop, 0);

    self->state = 0;
    self->recvop = 0;
}

void mill_call_tcpsocket_term (
    struct mill_coroutine_tcpsocket_term *cf,
    struct tcpsocket *self,
    struct mill_cfhead *parent,
    struct mill_loop *loop,
    int tag)
{
    assert (self->state != TCPSOCKET_STATE_TERMINATING);
    assert (self->recvop == 0);
    assert (self->sendop == 0);

    mill_cfhead_init (&cf->mill_cfhead, tcpsocket_term_handler,
        parent, loop, tag);

    self->state = TCPSOCKET_STATE_TERMINATING;
    self->recvop = &cf->mill_cfhead;
    uv_close ((uv_handle_t*) &self->s, tcpsocket_term_cb);
}

static void tcpsocket_connect_handler (
    struct mill_cfhead *base,
    struct mill_cfhead *event)
{
    struct mill_coroutine_tcpsocket_connect *cf;

    cf = mill_cont (base, struct mill_coroutine_tcpsocket_connect,
        mill_cfhead);
    assert (0);
}

static void tcpsocket_connect_cb (
    uv_connect_t* req,
    int status)
{
    struct mill_coroutine_tcpsocket_connect *cf;

    cf = mill_cont (req, struct mill_coroutine_tcpsocket_connect, req);

    assert (status == 0);

    assert (cf->self->state == TCPSOCKET_STATE_CONNECTING);
    mill_cfhead_emit (&cf->mill_cfhead, 0);

    cf->self->state = TCPSOCKET_STATE_ACTIVE;
    cf->self->recvop = 0;
}

void mill_call_tcpsocket_connect (
    struct mill_coroutine_tcpsocket_connect *cf,
    struct tcpsocket *self,
    struct sockaddr *addr,
    struct mill_cfhead *parent,
    struct mill_loop *loop,
    int tag)
{
    int rc;

    assert (self->state == TCPSOCKET_STATE_INIT);

    mill_cfhead_init (&cf->mill_cfhead, tcpsocket_connect_handler,
        parent, loop, tag);
    cf->self = self;
    rc = uv_tcp_connect (&cf->req, &self->s, addr, tcpsocket_connect_cb);
    assert (rc == 0);

    self->state = TCPSOCKET_STATE_CONNECTING;
    self->recvop = &cf->mill_cfhead;
}

int tcpsocket_bind (
    struct tcpsocket *self,
    struct sockaddr *addr,
    int flags)
{
    assert (self->state == TCPSOCKET_STATE_INIT);
    return uv_tcp_bind(&self->s, addr, flags);

    return 0;
}

static void tcpsocket_listen_cb (
    uv_stream_t *s,
    int status)
{
    int rc;
    struct tcpsocket *self;
    struct mill_coroutine_tcpsocket_accept *cf;
    uv_tcp_t uvs;

    self = mill_cont (s, struct tcpsocket, s);

    /* If nobody is accepting connections at the moment
       we'll simply drop them. */
    if (self->state != TCPSOCKET_STATE_ACCEPTING) {
        rc = uv_tcp_init (s->loop, &uvs);
        assert (rc == 0);
        rc = uv_accept (s, (uv_stream_t*) &uvs);
        assert (rc == 0);
        uv_close ((uv_handle_t*) &s, NULL); // TODO: This is an async op!
        return; 
    }

    /* Actual accept. */
    assert (self->recvop != 0);
    cf = mill_cont (self->recvop, struct mill_coroutine_tcpsocket_accept,
        mill_cfhead);
    rc = uv_accept (s, (uv_stream_t*) &cf->newsock->s);
    assert (rc == 0);
    cf->newsock->state = TCPSOCKET_STATE_ACTIVE;
    mill_cfhead_emit (&cf->mill_cfhead, 0);
    cf->self->state = TCPSOCKET_STATE_LISTENING;

    self->recvop = 0;
}

int tcpsocket_listen (
    struct tcpsocket *self,
    int backlog,
    struct mill_loop *loop)
{
    int rc;

    assert (self->state == TCPSOCKET_STATE_INIT);
    rc = uv_listen((uv_stream_t*) &self->s, backlog, tcpsocket_listen_cb);
    assert (rc == 0);
    self->state = TCPSOCKET_STATE_LISTENING;

    return 0;
}

static void tcpsocket_accept_handler (
    struct mill_cfhead *base,
    struct mill_cfhead *event)
{
    struct mill_coroutine_tcpsocket_accept *cf;

    cf = mill_cont (base, struct mill_coroutine_tcpsocket_accept, mill_cfhead);
    assert (0);
}

void mill_call_tcpsocket_accept (
    struct mill_coroutine_tcpsocket_accept *cf,
    struct tcpsocket *self,
    struct tcpsocket *newsock,
    struct mill_cfhead *parent,
    struct mill_loop *loop,
    int tag)
{
    assert (self->state == TCPSOCKET_STATE_LISTENING);

    mill_cfhead_init (&cf->mill_cfhead, tcpsocket_accept_handler,
        parent, loop, tag);
    cf->self = self;
    cf->newsock = newsock;

    self->state = TCPSOCKET_STATE_ACCEPTING;
    self->recvop = &cf->mill_cfhead;
}

static void tcpsocket_send_handler(
    struct mill_cfhead *base,
    struct mill_cfhead *event)
{
    struct mill_coroutine_tcpsocket_send *cf;

    cf = mill_cont (base, struct mill_coroutine_tcpsocket_send, mill_cfhead);
    assert (0);
}

static void tcpsocket_send_cb (
    uv_write_t* req,
    int status)
{
    struct mill_coroutine_tcpsocket_send *cf;

    cf = mill_cont (req, struct mill_coroutine_tcpsocket_send, req);

    assert (status == 0);
    cf->self->sendop = 0;
    mill_cfhead_emit (&cf->mill_cfhead, 0);
}

void mill_call_tcpsocket_send (
    struct mill_coroutine_tcpsocket_send *cf,
    struct tcpsocket *self,
    const void *buf,
    size_t len,
    struct mill_cfhead *parent,
    struct mill_loop *loop,
    int tag)
{
    int rc;

    assert (self->state == TCPSOCKET_STATE_ACTIVE);
    assert (self->sendop == 0);

    mill_cfhead_init (&cf->mill_cfhead, tcpsocket_send_handler,
        parent, loop, tag);
    cf->self = self;
    cf->buf.base = (void*) buf;
    cf->buf.len = len;

    rc = uv_write (&cf->req, (uv_stream_t*) &self->s, &cf->buf, 1,
        tcpsocket_send_cb);
    assert (rc == 0);
    self->sendop = &cf->mill_cfhead;
}

