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

#include "stdmill.h"

/******************************************************************************/
/* Generic helpers.                                                           */
/******************************************************************************/

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
/* msleep                                                                     */
/******************************************************************************/

static void msleep_timer_cb (
    uv_timer_t *timer);
static void msleep_close_cb (
    uv_handle_t *handle);

coroutine msleep (
    out int rc,
    int milliseconds)
{
    uv_timer_t timer;
    void *hndl;
    endvars;

    /* Start the timer. */
    rc = uv_timer_init (&cf->mill_cfh.loop->uv_loop, &timer);
    uv_assert (rc);
    rc = uv_timer_start (&timer, msleep_timer_cb, milliseconds, 0);
    uv_assert (rc);

    /* Wait till it finishes or the coroutine is canceled. */
    wait (&hndl);
    if (hndl == msleep_timer_cb)
        rc = 0;
    else if (hndl == 0)
        rc = ECANCELED;
    else
        assert (0);

    /* Close the timer. Ignore cancel requests during this phase. */
    uv_close ((uv_handle_t*) &timer, msleep_close_cb);
    while (1) {
        wait (&hndl);
        if (hndl == msleep_close_cb)
           break;
        assert (hndl == 0);
    }
}

static void msleep_timer_cb (
    uv_timer_t *timer)
{
    struct mill_cf_msleep *cf;

    cf = mill_cont (timer, struct mill_cf_msleep, timer);
    mill_handler_msleep (cf, (void*) msleep_timer_cb);
}

static void msleep_close_cb (
    uv_handle_t *handle)
{
    struct mill_cf_msleep *cf;

    cf = mill_cont (handle, struct mill_cf_msleep, timer);
    mill_handler_msleep (cf, (void*) msleep_close_cb);
}

/******************************************************************************/
/* tcpsocket                                                                  */
/******************************************************************************/

/* For the definition of struct tcpsocket look into stdmillx.h header file. */

static void tcpsocket_close_cb (
    uv_handle_t* handle);
static void tcpsocket_listen_cb (
    uv_stream_t *s,
    int status);
static void tcpsocket_connect_cb (
    uv_connect_t* req,
    int status);
static void tcpsocket_send_cb (
    uv_write_t* req,
    int status);
static void tcpsocket_alloc_cb (
    uv_handle_t* handle,
    size_t suggested_size,
    uv_buf_t* buf);
static void tcpsocket_recv_cb (
    uv_stream_t* stream,
    ssize_t nread,
    const uv_buf_t* buf);

int tcpsocket_init (
    struct tcpsocket *self,
    struct mill_loop *loop)
{
    self->loop = &loop->uv_loop;
    self->pc = 0;
    self->recvcfptr = 0;
    self->sendcfptr = 0;
    return uv_tcp_init (&loop->uv_loop, &self->s);
}

coroutine tcpsocket_term (
    struct tcpsocket *self)
{
    void *hndl;
    endvars;

    /* Initiate the termination. */
    self->recvcfptr = cf;
    uv_close ((uv_handle_t*) &self->s, tcpsocket_close_cb);

    /* Wait till socket is closed. In the meanime ignore cancel requests. */
    while (1) {
        wait (&hndl);
        if (hndl == tcpsocket_close_cb)
           break;
        assert (hndl == 0);
    }
}

int tcpsocket_bind (
    struct tcpsocket *self,
    struct sockaddr *addr,
    int flags)
{
    return uv_tcp_bind (&self->s, addr, flags);
}

int tcpsocket_listen (
    struct tcpsocket *self,
    int backlog)
{
    return uv_listen ((uv_stream_t*) &self->s, backlog, tcpsocket_listen_cb);
}

coroutine tcpsocket_connect (
    out int rc,
    struct tcpsocket *self,
    struct sockaddr *addr)
{
    uv_connect_t req;
    void *hndl;
    endvars;

    /* Start connecting. */
    self->recvcfptr = cf;
    rc = uv_tcp_connect (&req, &self->s, addr, tcpsocket_connect_cb);
    if (rc != 0)
        return;

    /* Wait till connecting finishes. */
    wait (&hndl);

    /* TODO: Canceling connect operation requires closing the entire socket. */
    if (!hndl) {
        assert (0);
    }

    assert (hndl == tcpsocket_connect_cb);
}

coroutine tcpsocket_accept (
    out int rc,
    struct tcpsocket *self,
    struct tcpsocket *newsock)
{
    void *hndl;
    endvars;

    /* Link the lisening socket with the accepting socket. */
    self->recvcfptr = cf;

    /* Wait for an incoming connection. */
    wait (&hndl);
    if (!hndl) {
        rc = ECANCELED;
        return;
    }
 
    /* There's a new incoming connection. Let's accept it. */
    assert (hndl == tcpsocket_listen_cb);
    rc = uv_accept ((uv_stream_t*) &self->s, (uv_stream_t*) &newsock->s);
}

coroutine tcpsocket_send (
    out int rc,
    struct tcpsocket *self,
    const void *buf,
    size_t len)
{
    uv_write_t req;
    uv_buf_t buffer;
    void *hndl;
    endvars;

    /* Start the send operation. */
    buffer.base = (void*) buf;
    buffer.len = len;
    rc = uv_write (&req, (uv_stream_t*) &self->s, &buffer, 1,
        tcpsocket_send_cb);
    if (rc != 0)
        return;

    /* Mark the socket as being in process of sending. */
    self->sendcfptr = cf;

    /* Wait till sending is done. */
    wait (&hndl);

    /* TODO: Cancelling a send operation requires closing the entire socket. */
    if (!hndl) {
        assert (0);
    }

    assert (hndl == tcpsocket_send_cb);
    self->sendcfptr = 0;
    rc = 0;
}

coroutine tcpsocket_recv (
    out int rc,
    struct tcpsocket *self,
    void *buf,
    size_t len)
{
    void *hndl;
    endvars;

    /* Sart the receiving. */
    rc = uv_read_start ((uv_stream_t*) &self->s,
        tcpsocket_alloc_cb, tcpsocket_recv_cb);
    if (rc != 0)
        return;

    /* Mark the socket as being in process of receiving. */
    cf->self->recvcfptr = cf;

    while (1) {

        /* Wait for next chunk of data. */
        wait (&hndl);

        /* User asks operation to be canceled. */
        if (!hndl) {
            uv_read_stop ((uv_stream_t*) &self->s);
            self->recvcfptr = 0;
            rc = ECANCELED;
            return;
        }

        /* If there are no more data to be read, stop reading. */
        if (!len) {
            uv_read_stop ((uv_stream_t*) &self->s);
            self->recvcfptr = 0;
            rc = 0;
            break;
        }
    }
}

static void tcpsocket_close_cb (
    uv_handle_t* handle)
{
    struct tcpsocket *self;
    struct mill_cf_tcpsocket_term *cf;

    self = mill_cont (handle, struct tcpsocket, s);
    cf = (struct mill_cf_tcpsocket_term*) self->recvcfptr;
    mill_handler_tcpsocket_term (cf, (void*) tcpsocket_close_cb);
}

static void tcpsocket_listen_cb (
    uv_stream_t *s,
    int status)
{
    struct tcpsocket *self;
    struct mill_cf_tcpsocket_accept *cf;

    self = mill_cont (s, struct tcpsocket, s);

    /* If nobody is accepting, close the incoming connection. */
    if (!self->recvcfptr) {
        assert (0); 
    }

    /* If somebody is accepting, move the accept coroutine on. */
    cf = (struct mill_cf_tcpsocket_accept*) self->recvcfptr;
    mill_handler_tcpsocket_accept (cf, (void*) tcpsocket_listen_cb);
}

static void tcpsocket_connect_cb (
    uv_connect_t* req,
    int status)
{
    struct mill_cf_tcpsocket_connect *cf;

    cf = mill_cont (req, struct mill_cf_tcpsocket_connect, req);
    mill_handler_tcpsocket_connect (cf, (void*) tcpsocket_connect_cb);
}

static void tcpsocket_send_cb (
    uv_write_t* req,
    int status)
{
    struct mill_cf_tcpsocket_send *cf;

    cf = mill_cont (req, struct mill_cf_tcpsocket_send, req);
    mill_handler_tcpsocket_send (cf, (void*) tcpsocket_send_cb);
}

static void tcpsocket_alloc_cb (
    uv_handle_t* handle,
    size_t suggested_size,
    uv_buf_t* buf)
{
    struct tcpsocket *self;
    struct mill_cf_tcpsocket_recv *cf;

    self = mill_cont (handle, struct tcpsocket, s);
    assert (self->recvcfptr);
    cf = (struct mill_cf_tcpsocket_recv*) self->recvcfptr;

    buf->base = cf->buf;
    buf->len = cf->len;
}

static void tcpsocket_recv_cb (
    uv_stream_t* stream,
    ssize_t nread,
    const uv_buf_t* buf)
{
    struct tcpsocket *self;
    struct mill_cf_tcpsocket_recv *cf;

    self = mill_cont (stream, struct tcpsocket, s);
    assert (self->recvcfptr);
    cf = (struct mill_cf_tcpsocket_recv*) self->recvcfptr;

    /* Adjust the input buffer to not cover the data already received. */
    assert (buf->base == cf->buf);
    assert (nread <= cf->len);
    cf->buf = ((char*) cf->buf) + nread;
    cf->len -= nread;

    mill_handler_tcpsocket_recv (cf, (void*) tcpsocket_recv_cb);
}

