/*

  Copyright (c) 2015 Martin Sustrik

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

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chan.h"
#include "cr.h"
#include "debug.h"
#include "libmill.h"
#include "utils.h"

MILL_CT_ASSERT(MILL_CLAUSELEN_ == sizeof(struct mill_clause));

static int mill_choose_seqnum = 0;

struct mill_chan_ *mill_getchan(struct mill_ep *ep) {
    switch(ep->type) {
    case MILL_SENDER:
        return mill_cont(ep, struct mill_chan_, sender);
    case MILL_RECEIVER:
        return mill_cont(ep, struct mill_chan_, receiver);
    default:
        assert(0);
    }
}

struct mill_chan_ *mill_chmake_(size_t sz, size_t bufsz, const char *created) {
    /* If there's at least one channel created in the user's code
       we want the debug functions to get into the binary. */
    mill_preserve_debug();
    /* We are allocating 1 additional element after the channel buffer to
       store the done-with value. It can't be stored in the regular buffer
       because that would mean chdone() would block when buffer is full. */
    struct mill_chan_ *ch = (struct mill_chan_*)
        malloc(sizeof(struct mill_chan_) + (sz * (bufsz + 1)));
    if(!ch)
        return NULL;
    mill_register_chan(&ch->debug, created);
    ch->sz = sz;
    ch->sender.type = MILL_SENDER;
    ch->sender.seqnum = mill_choose_seqnum;
    mill_list_init(&ch->sender.clauses);
    ch->receiver.type = MILL_RECEIVER;
    ch->receiver.seqnum = mill_choose_seqnum;
    mill_list_init(&ch->receiver.clauses);
    ch->refcount = 1;
    ch->done = 0;
    ch->bufsz = bufsz;
    ch->items = 0;
    ch->first = 0;
    mill_trace(created, "<%d>=chmake(%d)", (int)ch->debug.id, (int)bufsz);
    return ch;
}

struct mill_chan_ *mill_chdup_(struct mill_chan_ *ch, const char *current) {
    if(mill_slow(!ch))
        mill_panic("null channel used");
    mill_trace(current, "chdup(<%d>)", (int)ch->debug.id);
    ++ch->refcount;
    return ch;
}

void mill_chclose_(struct mill_chan_ *ch, const char *current) {
    if(mill_slow(!ch))
        mill_panic("null channel used");
    mill_trace(current, "chclose(<%d>)", (int)ch->debug.id);
    assert(ch->refcount > 0);
    --ch->refcount;
    if(ch->refcount)
        return;
    if(!mill_list_empty(&ch->sender.clauses) ||
          !mill_list_empty(&ch->receiver.clauses))
        mill_panic("attempt to close a channel while it is still being used");
    mill_unregister_chan(&ch->debug);
    free(ch);
}

/* Unblock a coroutine blocked in mill_choose_wait_() function.
   It also cleans up the associated clause list. */
static void mill_choose_unblock(struct mill_clause *cl) {
    struct mill_slist_item *it;
    struct mill_clause *itcl;
    for(it = mill_slist_begin(&cl->cr->choosedata.clauses);
          it; it = mill_slist_next(it)) {
        itcl = mill_cont(it, struct mill_clause, chitem);
        if(!itcl->used)
            continue;
        mill_list_erase(&itcl->ep->clauses, &itcl->epitem);
    }
    if(cl->cr->choosedata.ddline >= 0)
        mill_timer_rm(&cl->cr->timer);
    mill_resume(cl->cr, cl->idx);
}

static void mill_choose_init(const char *current) {
    mill_set_current(&mill_running->debug, current);
    mill_slist_init(&mill_running->choosedata.clauses);
    mill_running->choosedata.othws = 0;
    mill_running->choosedata.ddline = -1;
    mill_running->choosedata.available = 0;
    ++mill_choose_seqnum;
}

void mill_choose_init_(const char *current) {
    mill_trace(current, "choose()");
    mill_running->state = MILL_CHOOSE;
    mill_choose_init(current);
}

void mill_choose_in_(void *clause, struct mill_chan_ *ch, size_t sz, int idx) {
    if(mill_slow(!ch))
        mill_panic("null channel used");
    if(mill_slow(ch->sz != sz))
        mill_panic("receive of a type not matching the channel");
    /* Find out whether the clause is immediately available. */
    int available = ch->done || !mill_list_empty(&ch->sender.clauses) ||
        ch->items ? 1 : 0;
    if(available)
        ++mill_running->choosedata.available;
    /* If there are available clauses don't bother with non-available ones. */
    if(!available && mill_running->choosedata.available)
        return;
    /* Fill in the clause entry. */
    struct mill_clause *cl = (struct mill_clause*) clause;
    cl->cr = mill_running;
    cl->ep = &ch->receiver;
    cl->val = NULL;
    cl->idx = idx;
    cl->available = available;
    cl->used = 1;
    mill_slist_push_back(&mill_running->choosedata.clauses, &cl->chitem);
    if(cl->ep->seqnum == mill_choose_seqnum) {
        ++cl->ep->refs;
        return;
    }
    cl->ep->seqnum = mill_choose_seqnum;
    cl->ep->refs = 1;
    cl->ep->tmp = -1;
}

void mill_choose_out_(void *clause, struct mill_chan_ *ch, void *val, size_t sz,
      int idx) {
    if(mill_slow(!ch))
        mill_panic("null channel used");
    if(mill_slow(ch->done))
        mill_panic("send to done-with channel");
    if(mill_slow(ch->sz != sz))
        mill_panic("send of a type not matching the channel");
    /* Find out whether the clause is immediately available. */
    int available = !mill_list_empty(&ch->receiver.clauses) ||
        ch->items < ch->bufsz ? 1 : 0;
    if(available)
        ++mill_running->choosedata.available;
    /* If there are available clauses don't bother with non-available ones. */
    if(!available && mill_running->choosedata.available)
        return;
    /* Fill in the clause entry. */
    struct mill_clause *cl = (struct mill_clause*) clause;
    cl->cr = mill_running;
    cl->ep = &ch->sender;
    cl->val = val;
    cl->available = available;
    cl->idx = idx;
    cl->used = 1;
    mill_slist_push_back(&mill_running->choosedata.clauses, &cl->chitem);
    if(cl->ep->seqnum == mill_choose_seqnum) {
        ++cl->ep->refs;
        return;
    }
    cl->ep->seqnum = mill_choose_seqnum;
    cl->ep->refs = 1;
    cl->ep->tmp = -1;
}

static void mill_choose_callback(struct mill_timer *timer) {
    struct mill_cr *cr = mill_cont(timer, struct mill_cr, timer);
    struct mill_slist_item *it;
    for(it = mill_slist_begin(&cr->choosedata.clauses);
          it; it = mill_slist_next(it)) {
        struct mill_clause *itcl = mill_cont(it, struct mill_clause, chitem);
        mill_assert(itcl->used);
        mill_list_erase(&itcl->ep->clauses, &itcl->epitem);
    }
    mill_resume(cr, -1);
}

void mill_choose_deadline_(int64_t ddline) {
    if(mill_slow(mill_running->choosedata.othws ||
          mill_running->choosedata.ddline >= 0))
        mill_panic(
            "multiple 'otherwise' or 'deadline' clauses in a choose statement");
    /* Infinite deadline clause can never fire so we can as well ignore it. */
    if(ddline < 0)
        return;
    mill_running->choosedata.ddline = ddline;
}

void mill_choose_otherwise_(void) {
    if(mill_slow(mill_running->choosedata.othws ||
          mill_running->choosedata.ddline >= 0))
        mill_panic(
            "multiple 'otherwise' or 'deadline' clauses in a choose statement");
    mill_running->choosedata.othws = 1;
}

/* Push new item to the channel. */
static void mill_enqueue(struct mill_chan_ *ch, void *val) {
    /* If there's a receiver already waiting, let's resume it. */
    if(!mill_list_empty(&ch->receiver.clauses)) {
        mill_assert(ch->items == 0);
        struct mill_clause *cl = mill_cont(
            mill_list_begin(&ch->receiver.clauses), struct mill_clause, epitem);
        memcpy(mill_valbuf(cl->cr, ch->sz), val, ch->sz);
        mill_choose_unblock(cl);
        return;
    }
    /* Write the value to the buffer. */
    assert(ch->items < ch->bufsz);
    size_t pos = (ch->first + ch->items) % ch->bufsz;
    memcpy(((char*)(ch + 1)) + (pos * ch->sz) , val, ch->sz);
    ++ch->items;
}

/* Pop one value from the channel. */
static void mill_dequeue(struct mill_chan_ *ch, void *val) {
    /* Get a blocked sender, if any. */
    struct mill_clause *cl = mill_cont(
        mill_list_begin(&ch->sender.clauses), struct mill_clause, epitem);
    if(!ch->items) {
        /* If chdone was already called we can return the value immediately.
           There are no senders waiting to send. */
        if(mill_slow(ch->done)) {
            mill_assert(!cl);
            memcpy(val, ((char*)(ch + 1)) + (ch->bufsz * ch->sz), ch->sz);
            return;
        }
        /* Otherwise there must be a sender waiting to send. */
        mill_assert(cl);
        memcpy(val, cl->val, ch->sz);
        mill_choose_unblock(cl);
        return;
    }
    /* If there's a value in the buffer start by retrieving it. */
    memcpy(val, ((char*)(ch + 1)) + (ch->first * ch->sz), ch->sz);
    ch->first = (ch->first + 1) % ch->bufsz;
    --ch->items;
    /* And if there was a sender waiting, unblock it. */
    if(cl) {
        assert(ch->items < ch->bufsz);
        size_t pos = (ch->first + ch->items) % ch->bufsz;
        memcpy(((char*)(ch + 1)) + (pos * ch->sz) , cl->val, ch->sz);
        ++ch->items;
        mill_choose_unblock(cl);
    }
}

int mill_choose_wait_(void) {
    struct mill_choosedata *cd = &mill_running->choosedata;
    struct mill_slist_item *it;
    struct mill_clause *cl;

    /* If there are clauses that are immediately available
       randomly choose one of them. */
    if(cd->available > 0) {
        int chosen = cd->available == 1 ? 0 : (int)(random() % (cd->available));
        for(it = mill_slist_begin(&cd->clauses); it; it = mill_slist_next(it)) {
            cl = mill_cont(it, struct mill_clause, chitem);
            if(!cl->available)
                continue;
            if(!chosen)
                break;
            --chosen;
        }
        struct mill_chan_ *ch = mill_getchan(cl->ep);
        if(cl->ep->type == MILL_SENDER)
            mill_enqueue(ch, cl->val);
        else
            mill_dequeue(ch, mill_valbuf(cl->cr, ch->sz));
        mill_resume(mill_running, cl->idx);
        return mill_suspend();
    }

    /* If not so but there's an 'otherwise' clause we can go straight to it. */
    if(cd->othws) {
        mill_resume(mill_running, -1);
        return mill_suspend();
    }

    /* If deadline was specified, start the timer. */
    if(cd->ddline >= 0)
        mill_timer_add(&mill_running->timer, cd->ddline, mill_choose_callback);

    /* In all other cases register this coroutine with the queried channels
       and wait till one of the clauses unblocks. */
    for(it = mill_slist_begin(&cd->clauses); it; it = mill_slist_next(it)) {
        cl = mill_cont(it, struct mill_clause, chitem);
        if(mill_slow(cl->ep->refs > 1)) {
            if(cl->ep->tmp == -1)
                cl->ep->tmp =
                    cl->ep->refs == 1 ? 0 : (int)(random() % cl->ep->refs);
            if(cl->ep->tmp) {
                --cl->ep->tmp;
                cl->used = 0;
                continue;
            }
            cl->ep->tmp = -2;
        }
        mill_list_insert(&cl->ep->clauses, &cl->epitem, NULL);
    }
    /* If there are multiple parallel chooses done from different coroutines
       all but one must be blocked on the following line. */
    return mill_suspend();
}

void *mill_choose_val_(size_t sz) {
    /* The assumption here is that by supplying the same size as before
       we are going to get the same buffer which already has the data
       written into it. */
    return mill_valbuf(mill_running, sz);
}

void mill_chs_(struct mill_chan_ *ch, void *val, size_t sz,
      const char *current) {
    if(mill_slow(!ch))
        mill_panic("null channel used");
    mill_trace(current, "chs(<%d>)", (int)ch->debug.id);
    mill_choose_init(current);
    mill_running->state = MILL_CHS;
    struct mill_clause cl;
    mill_choose_out_(&cl, ch, val, sz, 0);
    mill_choose_wait_();
}

void *mill_chr_(struct mill_chan_ *ch, size_t sz, const char *current) {
    if(mill_slow(!ch))
        mill_panic("null channel used");
    mill_trace(current, "chr(<%d>)", (int)ch->debug.id);
    mill_running->state = MILL_CHR;
    mill_choose_init(current);
    struct mill_clause cl;
    mill_choose_in_(&cl, ch, sz, 0);
    mill_choose_wait_();
    return mill_choose_val_(sz);
}

void mill_chdone_(struct mill_chan_ *ch, void *val, size_t sz,
      const char *current) {
    if(mill_slow(!ch))
        mill_panic("null channel used");
    mill_trace(current, "chdone(<%d>)", (int)ch->debug.id);
    if(mill_slow(ch->done))
        mill_panic("chdone on already done-with channel");
    if(mill_slow(ch->sz != sz))
        mill_panic("send of a type not matching the channel");
    /* Panic if there are other senders on the same channel. */
    if(mill_slow(!mill_list_empty(&ch->sender.clauses)))
        mill_panic("send to done-with channel");
    /* Put the channel into done-with mode. */
    ch->done = 1;
    /* Store the terminal value into a special position in the channel. */
    memcpy(((char*)(ch + 1)) + (ch->bufsz * ch->sz) , val, ch->sz);
    /* Resume all the receivers currently waiting on the channel. */
    while(!mill_list_empty(&ch->receiver.clauses)) {
        struct mill_clause *cl = mill_cont(
            mill_list_begin(&ch->receiver.clauses), struct mill_clause, epitem);
        memcpy(mill_valbuf(cl->cr, ch->sz), val, ch->sz);
        mill_choose_unblock(cl);
    }
}

