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

#ifndef mill_h_included
#define mill_h_included

#include <assert.h>
#include <stdlib.h>
#include <uv.h>

/******************************************************************************/
/*  Coroutine metadata.                                                       */
/******************************************************************************/

/* Constant tag is used to make sure that the coframe is valid. If its type
   member doesn't point to something that begins with MILL_TYPE_TAG, the
   coframe can be considered invalid. */
#define mill_type_tag 0x4efd36df

/*  Special events. */
#define mill_event_init ((void*) 0)
#define mill_event_term ((void*) -1)
#define mill_event_done ((void*) -2)
#define mill_event_closed ((void*) -3)

/* 'cfptr' points to the coframe of the coroutine being evaluated.
   'event' either points to the coframe of the child coroutine that have just
   terminated or is one of the special events listed above. */
typedef void (*mill_fn_handler) (void *cfptr, void *event);

struct mill_type {
    int tag;
    mill_fn_handler handler;
};

/******************************************************************************/
/* Coframe head -- common to all coroutines.                                  */
/******************************************************************************/

/* These flags are used to construct 'flags' member. */
#define mill_flag_deallocate 1
#define mill_flag_canceled 2

struct mill_cfh {
    const struct mill_type *type;
    int state;
    int flags;
    int err;
    struct mill_cfh *parent;
    struct mill_cfh *children;
    struct mill_cfh *next;
    struct mill_cfh *prev;
    struct mill_loop *loop;
};

#define mill_callimpl_prologue(name)\
    struct mill_cf_##name *cf;\
    struct mill_cfh *mill_sibling;\
    int mill_flags = 0;\
    \
    cf = (struct mill_cf_##name*) cfptr;\
    if (!cf) {\
        mill_flags |= mill_flag_deallocate;\
        cf = malloc (sizeof (struct mill_cf_##name));\
        assert (cf);\
    }\
    cf->mill_cfh.type = type;\
    cf->mill_cfh.state = 0;\
    cf->mill_cfh.flags = mill_flags;\
    cf->mill_cfh.err = 0;\
    cf->mill_cfh.parent = parent;\
    cf->mill_cfh.children = 0;\
    cf->mill_cfh.next = 0;\
    cf->mill_cfh.prev = 0;\
    cf->mill_cfh.loop = loop;\
    if (cf->mill_cfh.parent) {\
        mill_sibling = cf->mill_cfh.parent->children;\
        cf->mill_cfh.next = mill_sibling;\
        if (mill_sibling)\
            mill_sibling->prev = &cf->mill_cfh;\
        cf->mill_cfh.parent->children = &cf->mill_cfh;\
    }

#define mill_callimpl_epilogue(name)\
    mill_handler_##name (&cf->mill_cfh, mill_event_init);\
    return (void*) cf;

#define mill_handlerimpl_prologue(name)\
    struct mill_cf_##name *cf;\
    struct mill_cfh *ev;\
    \
    cf = (struct mill_cf_##name*) cfptr;\
    ev = (struct mill_cfh*) event;\
    if (event == mill_event_term)\
        goto mill_finally;

#define mill_handlerimpl_epilogue(name, state)\
    mill_finally:\
    mill_cancelall (state);\
    mill_emit (cf);

#define mill_synccallimpl_prologue(name)\
    struct mill_loop loop;\
    struct mill_cf_##name cf;\
    int err;\
    \
    mill_loop_init (&loop);

#define mill_synccallimpl_epilogue(name)\
    mill_loop_run (&loop);\
    mill_loop_term (&loop);\
    mill_getresult (&cf, 0, &err);\
    return err;

/******************************************************************************/
/* The event loop. */
/******************************************************************************/

struct mill_loop
{
    /* Underlying libuv loop. */
    uv_loop_t uv_loop;

    /* Libuv hook that processes the mill events. */
    uv_idle_t idle;

    /* Local event queue. Items in this list are processed immediately,
       before control is returned to libuv. */
    struct mill_cfh *first;
    struct mill_cfh *last;
};

void mill_loop_init (struct mill_loop *self);
void mill_loop_term (struct mill_loop *self);
void mill_loop_run (struct mill_loop *self);
void mill_loop_emit (struct mill_loop *self, struct mill_cfh *base);

/******************************************************************************/
/*  Mill keywords.                                                            */
/******************************************************************************/

/*  wait  */

void mill_getresult (void *cfptr, void **who, int *err);

#define mill_wait(statearg, whoarg, errarg)\
    do {\
        cf->mill_cfh.state = (statearg);\
        return;\
        mill_state##statearg:\
        mill_getresult (event, (whoarg), (errarg));\
    } while (0)

/*  raise  */

#define mill_raise(errarg)\
    do {\
        cf->mill_cfh.err = (errarg);\
        goto mill_finally;\
    } while (0)

/*  return  */

#define mill_return mill_raise (0)

/*  cancel  */

void mill_cancel (void *cfptr);

/*  cancelall  */

void mill_cancel_children (void *cfptr);
int mill_has_children (void *cfptr);

#define mill_cancelall(statearg)\
    do {\
        mill_cancel_children (cf);\
        while (1) {\
            if (!mill_has_children (cf))\
                break;\
            cf->mill_cfh.state = (statearg);\
            return;\
            mill_state##statearg:\
            ;\
        }\
    } while (0)

/*  typeof  */

void *mill_typeof (void *cfptr);

/******************************************************************************/
/*  Coroutine msleep.                                                         */
/******************************************************************************/

/*
    coroutine msleep (
        int milliseconds);
*/

extern const struct mill_type mill_type_msleep;

struct mill_cf_msleep {

    /* Generic coframe header. */
    struct mill_cfh mill_cfh;

    /* Local variables. */
    uv_timer_t timer;
};

void *mill_call_msleep (
    void *cf,
    const struct mill_type *type,
    struct mill_loop *loop,
    void *parent,
    int millseconds);

int msleep (int milliseconds);

/******************************************************************************/
/*  Class tcpsocket.                                                          */
/******************************************************************************/

struct tcpsocket {
    uv_tcp_t s;
    uv_loop_t *loop;

    /* See TCPSOCKET_STATE_* constants. */
    int state;

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

/*
    coroutine tcpsocket_term (
        struct tcpsocket *self);
*/

extern const struct mill_type mill_type_tcpsocket_term;

struct mill_cf_tcpsocket_term {

    /* Generic coframe header. */
    struct mill_cfh mill_cfh;

    /* Coroutine arguments. */
    struct tcpsocket *self;
};

void *mill_call_tcpsocket_term (
    void *cfptr,
    const struct mill_type *type,
    struct mill_loop *loop,
    void *parent,
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

extern const struct mill_type mill_type_tcpsocket_connect;

struct mill_cf_tcpsocket_connect {

    /* Generic coframe header. */
    struct mill_cfh mill_cfh;

    /* Coroutine arguments. */
    struct tcpsocket *self;

    /* Local variables. */
    uv_connect_t req;
};

void *mill_call_tcpsocket_connect (
    void *cfptr,
    const struct mill_type *type,
    struct mill_loop *loop,
    void *parent,
    struct tcpsocket *self,
    struct sockaddr *addr);

int tcpsocket_listen (
    struct tcpsocket *self,
    int backlog);

/*
    coroutine tcpsocket_accept (
        struct tcpsocket *self,
        struct tcpsocket *newsock);
*/

extern const struct mill_type mill_type_tcpsocket_accept;

struct mill_cf_tcpsocket_accept {

    /* Generic coframe header. */
    struct mill_cfh mill_cfh;

    /* Coroutine arguments. */
    struct tcpsocket *self;
    struct tcpsocket *newsock;
};

void *mill_call_tcpsocket_accept (
    void *cfptr,
    const struct mill_type *type,
    struct mill_loop *loop,
    void *parent,
    struct tcpsocket *self,
    struct tcpsocket *newsock);

/*
    coroutine tcpsocket_send (
        struct tcpsocket *self,
        const void *buf,
        size_t len);
*/

extern const struct mill_type mill_type_tcpsocket_send;

struct mill_cf_tcpsocket_send {

    /* Generic coframe header. */
    struct mill_cfh mill_cfh;

    /* Coroutine arguments. */
    struct tcpsocket *self;

    /* Local variables. */
    uv_write_t req;
    uv_buf_t buffer;
};

void *mill_call_tcpsocket_send (
    void *cfptr,
    const struct mill_type *type,
    struct mill_loop *loop,
    void *parent,
    struct tcpsocket *self,
    const void *buf,
    size_t len);

/*
    coroutine tcpsocket_recv (
        struct tcpsocket *self,
        void *buf,
        size_t len);
*/

extern const struct mill_type mill_type_tcpsocket_recv;

struct mill_cf_tcpsocket_recv {

    /* Generic coframe header. */
    struct mill_cfh mill_cfh;

    /* Coroutine arguments. */
    struct tcpsocket *self;

    /* Local variables. */
    void *buf;
    size_t len;
};

void *mill_call_tcpsocket_recv (
    void *cfptr,
    const struct mill_type *type,
    struct mill_loop *loop,
    void *parent,
    struct tcpsocket *self,
    void *buf,
    size_t len);

#endif

