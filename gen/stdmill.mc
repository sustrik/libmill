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

#include "stdmill.mh"

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
/* Tracing support.                                                           */
/******************************************************************************/

static int mill_trace = 0;

void _mill_trace ()
{
    mill_trace = 1;
}

static void mill_printstack (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;
    if (cfh->parent) {
        mill_printstack (cfh->parent);
        fprintf (stderr, "/");
    }
    fprintf (stderr, "%s", cfh->type->name);
}

/******************************************************************************/
/* The event loop. */
/******************************************************************************/

static void loop_close_cb (uv_handle_t *handle)
{
    struct mill_loop *self;

    self = mill_cont (handle, struct mill_loop, idle);
    uv_stop (&self->uv_loop);
}

static void loop_cb (uv_idle_t* handle)
{
    struct mill_loop *self;
    struct mill_cfh *src;
    struct mill_cfh *dst;
    struct mill_cfh *it;
    struct mill_cfh *prev;

    self = mill_cont (handle, struct mill_loop, idle);

    while (self->first) {
        src = self->first;

        /*  If top level coroutine exits, we can stop the event loop.
            However, first we have to close the 'idle' object. */
        if (!src->parent) {

            /* Deallocate the coframe. */
            mill_coframe_term (src, 0);
            free (src);

            uv_close ((uv_handle_t*) &self->idle, loop_close_cb);
            return;
        }

        dst = src->parent;

        /* Invoke the handler for the finished coroutine. If the event can't
           be processed immediately.... */
        if (dst->type->handler (dst, src) != 0) {

            /* Remove it from the loop's event queue. */
            self->first = src->nextev;

            /* And put it to the end of parent's pending event queue. */
            src->nextev = NULL;
            if (dst->pfirst)
                dst->plast->nextev = src;
            else
                dst->pfirst = src;
            dst->plast = src;

            continue;
        }

        /* The event was processed successfully. That may have caused some of
           the pending events to become applicable. */
        it = dst->pfirst;
        prev = NULL;
        while (it != NULL) {

            /* Try to process the event. */
            if (dst->type->handler (dst, it) == 0) {

                /* Even was processed successfully. Remove it from the queue. */
                if (prev)
                    prev->nextev = it->nextev;
                else
                    dst->pfirst = it->nextev;
                if (!it->nextev)
                    dst->plast = prev;

                /* The coframe was already terminated by the handler.
                   Now deallocate the memory. */
                free (it);
                
                /* Start checking the pending event queue from beginning so
                   that older events are processed before newer ones. */
                it = dst->pfirst;
                prev = NULL;
                continue;
            }

            /* If the event isn't applicable, try the next one. */
            prev = it;
            it = it->nextev;
        }

        /* Move to the next event. */
        self->first = src->nextev;

        /* The coframe was already terminated by the handler.
           Now deallocate the memory. */
        free (src);
    }
    self->last = NULL;
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
    self->first = NULL;
    self->last = NULL;
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
    if (self->first == NULL)
        self->first = ev;
    else
        self->last->nextev = ev;
    ev->nextev = NULL;
    self->last = ev;
}

/******************************************************************************/
/*  Helpers used to implement mill keywords.                                  */
/******************************************************************************/

void mill_coframe_init (
    void *cfptr,
    const struct mill_type *type,
    void *parent,
    struct mill_loop *loop)
{
    struct mill_cfh *cfh;
    struct mill_cfh *pcfh;

    cfh = (struct mill_cfh*) cfptr;

    /*  Initialise the coframe. */
    cfh->type = type;
    cfh->pc = 0;
    cfh->nextev = NULL;
    cfh->pfirst = NULL;
    cfh->plast = NULL;
    cfh->parent = parent;
    cfh->children = NULL;
    cfh->next = NULL;
    cfh->prev = NULL;
    cfh->loop = loop;

    /* Add the coframe to the parent's list of child coroutines. */
    if (parent) {
        pcfh = (struct mill_cfh*) parent;
        cfh->prev = NULL;
        cfh->next = pcfh->children;
        if (pcfh->children)
            pcfh->children->prev = cfh;
        pcfh->children = cfh;
    }

    /* Trace start of the new coroutine. */
    if (mill_trace) {
        fprintf (stderr, "mill ==> go     ");
        mill_printstack (cfh);
        fprintf (stderr, "\n");
    }
}

void mill_coframe_term (
    void *cfptr,
    int canceled)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;

    /* Tracing support. No need to trace deallocation of the top-level coframe
       as the return from the coroutine is already reported and the two are
       basically the same thing. */
    if (mill_trace && cfh->parent) {
        fprintf (stderr, "mill ==> select ");
        mill_printstack (cfh);
        fprintf (stderr, "\n");
    }

    /* Copy the 'out' arguments to their final destinations. */
    if (!canceled)
        cfh->type->handler (cfh, cfh);

    /* Remove the coframe from the paren't list of child coroutines. */
    if (cfh->parent) {
        if (cfh->parent->children == cfh)
            cfh->parent->children = cfh->next;
        if (cfh->prev)
            cfh->prev->next = cfh->next;
        if (cfh->next)
            cfh->next->prev = cfh->prev;
        cfh->prev = NULL;
        cfh->next = NULL;
    }

    /* This is a heuristic that should cause an error if deallocated
       coframe is used. */
    cfh->type = NULL;
}

void mill_emit (
    void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;

    /* At this point all the child coroutines should be stopped. */
    assert (!cfh->children);

    /* Add the coroutine to the event queue. */
    mill_loop_emit (cfh->loop, cfh);

    if (mill_trace) {
        fprintf (stderr, "mill ==> return ");
        mill_printstack (cfh);
        fprintf (stderr, "\n");
    }
}

void mill_cancel_children (
    void *cfptr)
{
    struct mill_cfh *cfh;
    struct mill_cfh *child;

    cfh = (struct mill_cfh*) cfptr;

    /* Ask all child coroutines to cancel. */
    for (child = cfh->children; child != NULL; child = child->next) {
        if (mill_trace) {
            fprintf (stderr, "mill ==> cancel ");
            mill_printstack (child);
            fprintf (stderr, "\n");
        }
        child->type->handler (child, NULL);
    }
}

int mill_has_children (void *cfptr)
{
    struct mill_cfh *cfh;

    cfh = (struct mill_cfh*) cfptr;
    return cfh->children ? 1 : 0;
}

/******************************************************************************/
/* msleep                                                                     */
/******************************************************************************/

/* Forward declarations. */
static void msleep_timer_cb (
    uv_timer_t *timer);
static void msleep_close_cb (
    uv_handle_t *handle);

coroutine msleep (
    int milliseconds)
{
    int rc;
    uv_timer_t timer;
    endvars;

    /* Start the timer. */
    rc = uv_timer_init (&cf->mill_cfh.loop->uv_loop, &timer);
    uv_assert (rc);
    rc = uv_timer_start (&timer, msleep_timer_cb, milliseconds, 0);
    uv_assert (rc);

    /* Wait till it finishes or the coroutine is canceled. */
    syswait;
    assert (event == msleep_timer_cb || event == NULL);

    /* Close the timer. Ignore cancel requests during this phase. */
    uv_close ((uv_handle_t*) &timer, msleep_close_cb);
    while (1) {
        syswait;
        if (event == msleep_close_cb)
           break;
        assert (event == NULL);
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
/* getaddressinfo                                                             */
/******************************************************************************/

/* Forward declarations. */
static void getaddressinfo_cb (
    uv_getaddrinfo_t* req,
    int status,
    struct addrinfo* res);

coroutine getaddressinfo (
    out int *rc,
    const char *node,
    const char *service,
    const struct addrinfo *hints,
    out struct addrinfo **res)
{
    uv_getaddrinfo_t req;
    endvars;

    /* Start resolving the address in asynchronous manner. */
    /* TODO: Pass the 'hints' parameter to the function. When testing in
       on Linux/gcc I've seen a strage pointer there. A compiler bug? */
    *rc = uv_getaddrinfo (&cf->mill_cfh.loop->uv_loop, &req,
        getaddressinfo_cb, node, service, 0);
    uv_assert (*rc);

    /* Wait for next event. */
    syswait;

    /* If the coroutine ibs canceled by the caller. */
    if (event == NULL) {
        *rc = uv_cancel ((uv_req_t*) &req);
        uv_assert (*rc == 0);
        syswait;
    }

    /* If the coroutine have finished. */
    assert (event == getaddressinfo_cb);

cancel:
    uv_freeaddrinfo (*res);
}

static void getaddressinfo_cb (
    uv_getaddrinfo_t* req,
    int status,
    struct addrinfo* res)
{
    struct mill_cf_getaddressinfo *cf;

    cf = mill_cont (req, struct mill_cf_getaddressinfo, req);
    cf->rc = status;
    cf->res = res;
    mill_handler_getaddressinfo (cf, (void*) getaddressinfo_cb);
}

void freeaddressinfo (
    struct addrinfo *ai)
{
    uv_freeaddrinfo (ai);
}

/******************************************************************************/
/* tcpsocket                                                                  */
/******************************************************************************/

/* TODO: Isn't there an appropriate POSIX error? */
#define EFSM (-123456)

#define TCPSOCKET_STATE_INIT 1
#define TCPSOCKET_STATE_LISTENING 2
#define TCPSOCKET_STATE_CONNECTING 3
#define TCPSOCKET_STATE_ACCEPTING 4
#define TCPSOCKET_STATE_ACTIVE 5
#define TCPSOCKET_STATE_TERMINATING 6
#define TCPSOCKET_STATE_CANCELED 7

/* Forward declarations. */
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
    int rc;

    self->loop = &loop->uv_loop;
    self->state = TCPSOCKET_STATE_INIT;
    self->recvcfptr = NULL;
    self->sendcfptr = NULL;
    rc = uv_tcp_init (&loop->uv_loop, &self->s);
    if (rc != 0)
        return rc;
}

coroutine tcpsocket_term (
    struct tcpsocket *self)
{
    /* Initiate the termination. */
    self->recvcfptr = cf;
    uv_close ((uv_handle_t*) &self->s, tcpsocket_close_cb);
    self->state = TCPSOCKET_STATE_TERMINATING;

    /* Wait till socket is closed. In the meantime ignore cancel requests. */
    while (1) {
        syswait;
        if (event == tcpsocket_close_cb)
           break;
        assert (event == NULL);
    }
}

int tcpsocket_bind (
    struct tcpsocket *self,
    struct sockaddr *addr,
    int flags)
{
    if (self->state != TCPSOCKET_STATE_INIT)
        return EFSM;

    return uv_tcp_bind (&self->s, addr, flags);
}

int tcpsocket_listen (
    struct tcpsocket *self,
    int backlog)
{
    int rc;

    if (self->state != TCPSOCKET_STATE_INIT)
        return EFSM;

    rc = uv_listen ((uv_stream_t*) &self->s, backlog, tcpsocket_listen_cb);
    if (rc != 0)
        return rc;
    self->state = TCPSOCKET_STATE_LISTENING;
}

coroutine tcpsocket_connect (
    out int *rc,
    struct tcpsocket *self,
    struct sockaddr *addr)
{
    uv_connect_t req;
    endvars;

    if (self->state != TCPSOCKET_STATE_INIT) {
        *rc = EFSM;
        return;
    }

    /* Start connecting. */
    self->recvcfptr = cf;
    *rc = uv_tcp_connect (&req, &self->s, addr, tcpsocket_connect_cb);
    if (*rc != 0)
        return;
    self->state = TCPSOCKET_STATE_CONNECTING;

    /* Wait till connecting finishes. */
    syswait;

    /* TODO: Canceling connect operation requires closing the entire socket. */
    if (!event) {
        assert (0);
    }

    assert (event == tcpsocket_connect_cb);
    /* TODO: What about errors? */
    self->state = TCPSOCKET_STATE_ACTIVE;
}

coroutine tcpsocket_accept (
    out int *rc,
    struct tcpsocket *self,
    struct tcpsocket *from)
{
    if (self->state != TCPSOCKET_STATE_INIT ||
          from->state != TCPSOCKET_STATE_LISTENING ||
          from->recvcfptr != NULL) {
        *rc = EFSM;
        return;
    }

    while (1) {

        *rc = uv_accept ((uv_stream_t*) &from->s, (uv_stream_t*) &self->s);
        /* TODO: Handle errors. */
        if (*rc == 0)
           break;

        /* Link the lisening socket with the accepting socket and wait
           for incoming connection. */
        from->recvcfptr = cf;
        self->state = TCPSOCKET_STATE_ACCEPTING;

        /* Wait for an incoming connection. */
        syswait;
        if (!event) {
            self->state = TCPSOCKET_STATE_INIT;
            return;
        }

        /* There's a new incoming connection. Let's accept it. */
        assert (event == tcpsocket_listen_cb);
    }

    /* Connection was successfully accepted. */
    self->state = TCPSOCKET_STATE_ACTIVE;
}

coroutine tcpsocket_send (
    out int *rc,
    struct tcpsocket *self,
    const void *buf,
    size_t len)
{
    uv_write_t req;
    uv_buf_t buffer;
    endvars;

    if (self->state != TCPSOCKET_STATE_ACTIVE) {
        *rc = EFSM;
        return;
    }

    /* Start the send operation. */
    buffer.base = (void*) buf;
    buffer.len = len;
    *rc = uv_write (&req, (uv_stream_t*) &self->s, &buffer, 1,
        tcpsocket_send_cb);
    if (*rc != 0)
        return;

    /* Mark the socket as being in process of sending. */
    self->sendcfptr = cf;

    /* Wait till sending is done. */
    syswait;

    /* TODO: Cancelling a send operation requires closing the entire socket. */
    if (!event) {
        assert (0);
    }

    assert (event == tcpsocket_send_cb);
    self->sendcfptr = NULL;
    *rc = 0;
}

coroutine tcpsocket_recv (
    out int *rc,
    struct tcpsocket *self,
    void *buf,
    size_t len)
{
    if (self->state != TCPSOCKET_STATE_ACTIVE) {
        *rc = EFSM;
        return;
    }

    /* Start receiving. */
    *rc = uv_read_start ((uv_stream_t*) &self->s,
        tcpsocket_alloc_cb, tcpsocket_recv_cb);
    if (*rc != 0)
        return;

    /* Mark the socket as being in process of receiving. */
    cf->self->recvcfptr = cf;

    while (1) {

        /* Wait for next chunk of data. */
        syswait;

        /* User asks operation to be canceled. */
        if (!event) {
            uv_read_stop ((uv_stream_t*) &self->s);
            self->recvcfptr = NULL;
            return;
        }

        /* If there are no more data to be read, stop reading. */
        if (!len) {
            uv_read_stop ((uv_stream_t*) &self->s);
            self->recvcfptr = NULL;
            *rc = 0;
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

