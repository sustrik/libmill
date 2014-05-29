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
#include <stdlib.h>

#define mill_cont(ptr, type, member) \
    (ptr ? ((type*) (((char*) ptr) - offsetof(type, member))) : NULL)

#define uv_assert(x)\
    do {\
        if (x < 0) {\
            fprintf (stderr, "%s: %s (%s:%d)\n", uv_err_name (x),\
                uv_strerror (x), __FILE__, __LINE__);\
            fflush (stderr);\
            abort ();\
        }\
    } while (0)

/******************************************************************************/
/* The event loop. */
/******************************************************************************/

static void loop_cb (uv_idle_t* handle)
{
    struct mill_loop *self;
    struct mill_cfh *src;

    self = mill_cont (handle, struct mill_loop, idle);

    while (self->first) {
        src = self->first;

        if (!src->parent) {
            uv_stop (&self->uv_loop);
            return;
        }

        /* Remove the child from parent's list of children. */
        if (src != 0 && src != (struct mill_cfh*) -1) {
            if (src->parent->children == src)
                src->parent->children = src->next;
            if (src->prev)
                src->prev->next = src->next;
            if (src->next)
                src->next->prev = src->prev;
        }

        src->parent->type->handler (src->parent, src);
        self->first = src->next;

        /* Deallocate auto-allocated coframes. */
        if (src->flags & mill_flag_deallocate)
            free (src);
    }
    self->last = 0;
}

void mill_loop_init (
    struct mill_loop *self)
{
    int rc;

    rc = uv_loop_init (&self->uv_loop);
    uv_assert (rc);
    rc = uv_idle_init (&self->uv_loop, &self->idle);
    uv_assert (rc);
    rc = uv_idle_start (&self->idle, loop_cb);
    uv_assert (rc);
    self->first = 0;
    self->last = 0;
}

void mill_loop_term (
    struct mill_loop *self)
{
    int rc;

    rc = uv_idle_stop (&self->idle);
    uv_assert (rc);
    rc = uv_loop_close (&self->uv_loop);
    uv_assert (rc);
}

void mill_loop_run (
    struct mill_loop *self)
{
    int rc;

    rc = uv_run (&self->uv_loop, UV_RUN_DEFAULT);
    uv_assert (rc);
}

void mill_loop_emit (
    struct mill_loop *self,
    struct mill_cfh *ev)
{
    if (self->first == 0)
        self->first = ev;
    else
        self->last->next = ev;
    ev->next = 0;
    self->last = ev;
}

/******************************************************************************/
/*  Mill keywords.                                                            */
/******************************************************************************/

void mill_getresult (void *cfptr, void **who, int *err)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;
    if (who)
        *who = (void*) cfh;
    if (err)
        *err = cfh->err;
}

void mill_cancel (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;
    assert (cfh->type->tag == mill_type_tag);
    cfh->type->handler (cfh, mill_event_term);
}

void *mill_typeof (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;
    assert (cfh->type && cfh->type->tag == mill_type_tag);
    return (void*) cfh->type;
}

void mill_emit (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr; 
    mill_loop_emit (cfh->loop, cfh);
}

void mill_cancel_children (void *cfptr)
{
    struct mill_cfh *cfh;
    struct mill_cfh *child;

    cfh = (struct mill_cfh*) cfptr; 
    for (child = cfh->children; child != 0; child = child->next)
        child->type->handler (child,  mill_event_term);
}

int mill_has_children (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;
    return cfh->children ? 1 : 0;
}

/******************************************************************************/
/*  Coroutine msleep.                                                         */
/******************************************************************************/

static void mill_handler_msleep (
    void *cfptr,
    void *event)
{
    int rc;
    struct mill_cf_msleep *cf;

    cf = (struct mill_cf_msleep*) cfptr;

    if (event == mill_event_init)
        return;

    /* Cancel the timer. */
    if (event == mill_event_term) {
        rc = uv_timer_stop (&cf->timer);
        uv_assert (rc);
        cf->mill_cfh.err = ECANCELED;
        mill_emit (cf);
        return;
    }
}

static void msleep_cb (
    uv_timer_t *timer)
{
    struct mill_cf_msleep *cf;

    cf = mill_cont (timer, struct mill_cf_msleep, timer);

    /* The coroutine is done. */
    mill_emit (cf);
}

const struct mill_type mill_type_msleep =
    {mill_type_tag, mill_handler_msleep};

void *mill_call_msleep (
    void *cfptr,
    const struct mill_type *type,
    struct mill_loop *loop,
    void *parent,
    int milliseconds)
{
    mill_callimpl_prologue (msleep);

    /* Launch the timer. */
    uv_timer_init (&loop->uv_loop, &cf->timer);
    uv_timer_start(&cf->timer, msleep_cb, milliseconds, 0);

    mill_callimpl_epilogue (msleep);
}

int msleep (int milliseconds)
{
    assert (0);
}

/******************************************************************************/
/* Class tcpsocket.                                                           */
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
    self->recvcfptr = 0;
    self->sendcfptr = 0;
    return uv_tcp_init (&loop->uv_loop, &self->s);
}

static void mill_handler_tcpsocket_term (
    void *cfptr,
    void *event)
{
    mill_handlerimpl_prologue (tcpsocket_term);

    if (event == mill_event_init)
        return;
    if (event == mill_event_term)
        assert (0); // TODO
    assert (event == mill_event_done);

    mill_handlerimpl_epilogue (tcpsocket_term, 1);
}

static void tcpsocket_term_cb (
    uv_handle_t* handle)
{
    struct tcpsocket *self = mill_cont (handle, struct tcpsocket, s);

    /* Double check that the socket is in the correct state. */
    assert (self->state == TCPSOCKET_STATE_TERMINATING);
    assert (self->recvcfptr != 0);

    /* The coroutine is done. */
    mill_handler_tcpsocket_term (self->recvcfptr, mill_event_done);

    /* Flip the state. */
    self->state = 0;
    self->recvcfptr = 0;
}

const struct mill_type mill_type_tcpsocket_term =
    {mill_type_tag, mill_handler_tcpsocket_term};

void *mill_call_tcpsocket_term (
    void *cfptr,
    const struct mill_type *type,
    struct mill_loop *loop,
    void *parent,
    struct tcpsocket *self)
{
    mill_callimpl_prologue (tcpsocket_term);

    cf->self = self;

    /* Make sure that no async operations are going on on the socket. */
    assert (cf->self->state != TCPSOCKET_STATE_TERMINATING);
    assert (cf->self->recvcfptr == 0);
    assert (cf->self->sendcfptr == 0);

    /* Mark the socket as being in process of being terminated. */
    cf->self->state = TCPSOCKET_STATE_TERMINATING;
    cf->self->recvcfptr = &cf->mill_cfh;

    /* Initiate the termination. */
    uv_close ((uv_handle_t*) &cf->self->s, tcpsocket_term_cb);

    mill_callimpl_epilogue (tcpsocket_term);
}

static void mill_handler_tcpsocket_connect (
    void *cfptr,
    void *event)
{
    mill_handlerimpl_prologue (tcpsocket_connect);

    mill_handlerimpl_epilogue (tcpsocket_connect, 1);
}

static void tcpsocket_connect_cb (
    uv_connect_t* req,
    int status)
{
    struct mill_cf_tcpsocket_connect *cf;

    cf = mill_cont (req, struct mill_cf_tcpsocket_connect, req);

    /* Double-check that the socket is in the correct state. */
    assert (cf->self->state == TCPSOCKET_STATE_CONNECTING);
    assert (cf->self->recvcfptr != 0);

    /* The coroutine is done. */
    cf->mill_cfh.err = status;
    mill_handler_tcpsocket_connect (cf->self->recvcfptr, mill_event_done);
    
    /* Flip the state. */
    cf->self->state = (status == 0) ?
        TCPSOCKET_STATE_ACTIVE :
        TCPSOCKET_STATE_INIT;
    cf->self->recvcfptr = 0;    
}

const struct mill_type mill_type_tcpsocket_connect =
    {mill_type_tag, mill_handler_tcpsocket_connect};

void *mill_call_tcpsocket_connect (
    void *cfptr,
    const struct mill_type *type,
    struct mill_loop *loop,
    void *parent,
    struct tcpsocket *self,
    struct sockaddr *addr)
{
    int rc;

    mill_callimpl_prologue (tcpsocket_connect);

    cf->self = self;

    /* Connect can be done only on a fresh socket. */
    assert (cf->self->state == TCPSOCKET_STATE_INIT);

    /* Mark the socket as being in process of connecting. */
    cf->self->state = TCPSOCKET_STATE_CONNECTING;
    cf->self->recvcfptr = cf;
    
    /* Initiate the connecting. */
    rc = uv_tcp_connect (&cf->req, &cf->self->s, addr, tcpsocket_connect_cb);
    uv_assert (rc);

    mill_callimpl_epilogue (tcpsocket_connect);
}

int tcpsocket_bind (
    struct tcpsocket *self,
    struct sockaddr *addr,
    int flags)
{
    /* Socket can be bound only before it is listening or connected. */
    assert (self->state == TCPSOCKET_STATE_INIT);

    return uv_tcp_bind (&self->s, addr, flags);
}

static void tcpsocket_listen_cb (
    uv_stream_t *s,
    int status)
{
    int rc;
    struct tcpsocket *self;
    struct mill_cf_tcpsocket_accept *cf;
    uv_tcp_t uvs;

    self = mill_cont (s, struct tcpsocket, s);

    /* Double-check that the socket is in the correct state. */
    assert (self->state == TCPSOCKET_STATE_LISTENING ||
        self->state == TCPSOCKET_STATE_ACCEPTING ||
        self->state == TCPSOCKET_STATE_TERMINATING);

    /* If nobody is accepting connections at the moment
       we'll simply drop them. */
    if (self->state != TCPSOCKET_STATE_ACCEPTING) {
        rc = uv_tcp_init (s->loop, &uvs);
        uv_assert (rc);
        rc = uv_accept (s, (uv_stream_t*) &uvs);
        uv_assert (rc);
        uv_close ((uv_handle_t*) &s, NULL); // TODO: This is an async op!

        /* The coroutine goes on. No need to notify the parent. */
        return; 
    }

    if (status != 0)
        goto error;

    /* Actual accept. */
    assert (self->recvcfptr != 0);
    cf = mill_cont (self->recvcfptr, struct mill_cf_tcpsocket_accept,
        mill_cfh);
    status = uv_accept (s, (uv_stream_t*) &cf->newsock->s);
    if (status != 0)
        goto error;

    /* Flip both listening and accepted socket to new states. */
    cf->newsock->state = TCPSOCKET_STATE_ACTIVE;
    cf->self->state = TCPSOCKET_STATE_LISTENING;
    self->recvcfptr = 0;

    /* The coroutine is done. */
    mill_emit (cf);
    return;

error:

    /* End the coroutine. Report the error. */
    cf->newsock->state = TCPSOCKET_STATE_INIT;
    cf->self->state = TCPSOCKET_STATE_LISTENING;
    self->recvcfptr = 0;
    cf->mill_cfh.err = status;
    mill_emit (cf);
}

int tcpsocket_listen (
    struct tcpsocket *self,
    int backlog)
{
    int rc;

    /* Listen cannot be called on socket that is already listening or
       connected. */
    assert (self->state == TCPSOCKET_STATE_INIT);

    /* Start listening. */
    rc = uv_listen((uv_stream_t*) &self->s, backlog, tcpsocket_listen_cb);
    if (rc != 0)
        return rc;
    self->state = TCPSOCKET_STATE_LISTENING;
    return 0;
}

static void mill_handler_tcpsocket_accept (void *cfptr, void *event)
{
    mill_handlerimpl_prologue (tcpsocket_accept);

    if (event == mill_event_init)
        return;
    else if (event == mill_event_term) {
        cf->newsock->state = TCPSOCKET_STATE_INIT;
        cf->self->state = TCPSOCKET_STATE_LISTENING;
        cf->self->recvcfptr = 0;
        cf->mill_cfh.err = ECANCELED;
    }
    else
        assert (event == mill_event_done);

    mill_handlerimpl_epilogue (tcpsocket_accept, 1);
}

const struct mill_type mill_type_tcpsocket_accept =
    {mill_type_tag, mill_handler_tcpsocket_accept};

void *mill_call_tcpsocket_accept (
    void *cfptr,
    const struct mill_type *type,
    struct mill_loop *loop,
    void *parent,
    struct tcpsocket *self,
    struct tcpsocket *newsock)
{
    mill_callimpl_prologue (tcpsocket_accept);
    cf->self = self;
    cf->newsock = newsock;

    /* Only sockets that are already listening can accept new connections. */
    assert (cf->self->state == TCPSOCKET_STATE_LISTENING);

    /* Mark the socket as being in process of being accepted. */
    cf->self->state = TCPSOCKET_STATE_ACCEPTING;
    cf->self->recvcfptr = cf;

    /* There's no need for any action here as callback for incoming connections
       was already registered in tcpsocket_listen function. */

    mill_callimpl_epilogue (tcpsocket_accept);
}

static void mill_handler_tcpsocket_send (void *cfptr, void *event)
{
    mill_handlerimpl_prologue (tcpsocket_send);



    mill_handlerimpl_epilogue (tcpsocket_send, 1);
}

static void tcpsocket_send_cb (
    uv_write_t* req,
    int status)
{
    struct mill_cf_tcpsocket_send *cf;

    cf = mill_cont (req, struct mill_cf_tcpsocket_send, req);

    /* Notify the parent coroutine that the operation is finished. */
    cf->self->sendcfptr = 0;
    cf->mill_cfh.err = status;
    mill_emit (cf);
}

const struct mill_type mill_type_tcpsocket_send =
    {mill_type_tag, mill_handler_tcpsocket_send};

void *mill_call_tcpsocket_send (
    void *cfptr,
    const struct mill_type *type,
    struct mill_loop *loop,
    void *parent,
    struct tcpsocket *self,
    const void *buf,
    size_t len)
{
    int rc;

    mill_callimpl_prologue (tcpsocket_send);

    cf->self = self;

    /* Make sure that the socket is properly connected and that no other send
       operation is in progress. */
    assert (cf->self->state == TCPSOCKET_STATE_ACTIVE);
    assert (cf->self->sendcfptr == 0);

    /* Mark the socket as being in process of sending. */
    cf->self->sendcfptr = cf;

    /* Initiate the sending. */
    cf->buffer.base = (void*) buf;
    cf->buffer.len = len;
    rc = uv_write (&cf->req, (uv_stream_t*) &cf->self->s, &cf->buffer, 1,
        tcpsocket_send_cb);
    assert (rc == 0);

    mill_callimpl_epilogue (tcpsocket_send);
}

static void mill_handler_tcpsocket_recv (void *cfptr, void *event)
{
    int rc;

    mill_handlerimpl_prologue (tcpsocket_recv);

    if (event == mill_event_init)
        return;
    else if (event == mill_event_term) {
        rc = uv_read_stop ((uv_stream_t*) &cf->self->s);
        assert (rc == 0);
        cf->self->recvcfptr = 0;
        cf->mill_cfh.err = ECANCELED;
    }
    else
        assert (event == mill_event_done);

    mill_handlerimpl_epilogue (tcpsocket_recv, 1);
}

static void tcpsocket_alloc_cb (
    uv_handle_t* handle,
    size_t suggested_size,
    uv_buf_t* buf)
{
    struct mill_cf_tcpsocket_recv *cf;
    struct tcpsocket *self;

    self = mill_cont (handle, struct tcpsocket, s);
    assert (self->recvcfptr);
    cf = mill_cont (self->recvcfptr, struct mill_cf_tcpsocket_recv,
        mill_cfh);

    buf->base = cf->buf;
    buf->len = cf->len;
}

static void tcpsocket_recv_cb (
    uv_stream_t* stream,
    ssize_t nread,
    const uv_buf_t* buf)
{
    struct mill_cf_tcpsocket_recv *cf;
    struct tcpsocket *self;

    self = mill_cont (stream, struct tcpsocket, s);
    assert (self->recvcfptr);
    cf = mill_cont (self->recvcfptr, struct mill_cf_tcpsocket_recv,
        mill_cfh);

    /* Adjust the input buffer to not cover the data already received. */
    assert (buf->base == cf->buf);
    assert (nread <= cf->len);
    cf->buf = ((char*) cf->buf) + nread;
    cf->len -= nread;

    /* If there are no more data to be read, stop reading and notify the parent
       coroutine that the operation is finished. */
    if (!cf->len) {
        uv_read_stop (stream);
        self->recvcfptr = 0;
        mill_emit (cf);
    }
}

const struct mill_type mill_type_tcpsocket_recv =
    {mill_type_tag, mill_handler_tcpsocket_recv};

void *mill_call_tcpsocket_recv (
    void *cfptr,
    const struct mill_type *type,
    struct mill_loop *loop,
    void *parent,
    struct tcpsocket *self,
    void *buf,
    size_t len)
{
    int rc;

    mill_callimpl_prologue (tcpsocket_recv);

    cf->self = self;
    cf->buf = buf;
    cf->len = len;

    /* Make sure that the socket is properly connected and that no other recv
       operation is in progress. */
    assert (cf->self->state == TCPSOCKET_STATE_ACTIVE);
    assert (cf->self->recvcfptr == 0);

    /* Mark the socket as being in process of receiving. */
    cf->self->recvcfptr = cf;

    /* Initiate the receiving. */
    rc = uv_read_start ((uv_stream_t*) &cf->self->s,
        tcpsocket_alloc_cb, tcpsocket_recv_cb);
    uv_assert (rc);

    mill_callimpl_epilogue (tcpsocket_recv);
}

