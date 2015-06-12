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

#include "cr.h"
#include "debug.h"
#include "libmill.h"
#include "model.h"
#include "utils.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MILL_CT_ASSERT(MILL_CLAUSELEN == sizeof(struct mill_clause));

chan mill_chmake(size_t sz, size_t bufsz, const char *created) {
    /* If there's at least one channel created in the user's code
       we want the debug functions to get into the binary. */
    mill_preserve_debug();
    /* We are allocating 1 additional element after the channel buffer to
       store the done-with value. It can't be stored in the regular buffer
       because that would mean chdone() would block when buffer is full. */
    struct mill_chan *ch = (struct mill_chan*)
        malloc(sizeof(struct mill_chan) + (sz * (bufsz + 1)));
    assert(ch);
    mill_register_chan(&ch->debug, created);
    ch->sz = sz;
    ch->sender.type = MILL_SENDER;
    mill_list_init(&ch->sender.clauses);
    ch->receiver.type = MILL_RECEIVER;
    mill_list_init(&ch->receiver.clauses);
    ch->refcount = 1;
    ch->done = 0;
    ch->bufsz = bufsz;
    ch->items = 0;
    ch->first = 0;
    mill_trace(created, "<%d>=chmake(%d)", (int)ch->debug.id, (int)bufsz);
    return ch;
}

chan mill_chdup(chan ch, const char *current) {
    mill_trace(current, "chdup(<%d>)", (int)ch->debug.id);
    ++ch->refcount;
    return ch;
}

void mill_chclose(chan ch, const char *current) {
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

static void mill_choose_init_(const char *current) {
    mill_set_current(&mill_running->debug, current);
    mill_slist_init(&mill_running->u_choose.clauses);
    mill_running->u_choose.othws = 0;
    mill_running->u_choose.available = 0;
}

void mill_choose_init(const char *current) {
    mill_trace(current, "choose()");
    mill_running->state = MILL_CHOOSE;
    mill_choose_init_(current);
}

void mill_choose_in(void *clause, chan ch, size_t sz, int idx) {
    if(mill_slow(ch->sz != sz))
        mill_panic("receive of a type not matching the channel");
    /* Find out whether the clause is immediately available. */
    int available = ch->done || !mill_list_empty(&ch->sender.clauses) ||
        ch->items ? 1 : 0;
    if(available)
        ++mill_running->u_choose.available;
    /* If there are available clauses don't bother with non-available ones. */
    if(!available && mill_running->u_choose.available)
        return;
    /* Fill in the clause entry. */
    struct mill_clause *cl = (struct mill_clause*) clause;
    cl->cr = mill_running;
    cl->ep = &ch->receiver;
    cl->val = NULL;
    cl->idx = idx;
    cl->available = available;
    mill_slist_push_back(&mill_running->u_choose.clauses, &cl->chitem);
    /* Add the clause to the channel's list of waiting receivers. */
    mill_list_insert(&ch->receiver.clauses, &cl->epitem, NULL);
}

void mill_choose_out(void *clause, chan ch, void *val, size_t sz, int idx) {
    if(mill_slow(ch->done))
        mill_panic("send to done-with channel");
    if(mill_slow(ch->sz != sz))
        mill_panic("send of a type not matching the channel");
    /* Find out whether the clause is immediately available. */
    int available = !mill_list_empty(&ch->receiver.clauses) ||
        ch->items < ch->bufsz ? 1 : 0;
    if(available)
        ++mill_running->u_choose.available;
    /* If there are available clauses don't bother with non-available ones. */
    if(!available && mill_running->u_choose.available)
        return;
    /* Fill in the clause entry. */
    struct mill_clause *cl = (struct mill_clause*) clause;
    cl->cr = mill_running;
    cl->ep = &ch->sender;
    cl->val = val;
    cl->available = available;
    cl->idx = idx;
    mill_slist_push_back(&mill_running->u_choose.clauses, &cl->chitem);
    /* Add the clause to the channel's list of waiting senders. */
    mill_list_insert(&ch->sender.clauses, &cl->epitem, NULL);
}

void mill_choose_otherwise(void) {
    if(mill_slow(mill_running->u_choose.othws != 0))
        mill_panic("multiple 'otherwise' clauses in a choose statement");
    mill_running->u_choose.othws = 1;
}

/* Push new item to the channel. */
static void mill_enqueue(chan ch, void *val) {
    /* If there's a receiver already waiting, let's resume it. */
    if(!mill_list_empty(&ch->receiver.clauses)) {
        struct mill_clause *cl = mill_cont(
            mill_list_begin(&ch->receiver.clauses), struct mill_clause, epitem);
        memcpy(mill_valbuf_alloc(&cl->cr->valbuf, ch->sz), val, ch->sz);
        mill_resume(cl->cr, cl->idx);
        mill_list_erase(&ch->receiver.clauses, &cl->epitem);
        return;
    }
    /* Write the value to the buffer. */
    assert(ch->items < ch->bufsz);
    size_t pos = (ch->first + ch->items) % ch->bufsz;
    memcpy(((char*)(ch + 1)) + (pos * ch->sz) , val, ch->sz);
    ++ch->items;
}

/* Pop one value from the channel. */
static void mill_dequeue(chan ch) {
    void *dst = mill_valbuf_alloc(
            &mill_cont(mill_list_begin(&ch->receiver.clauses),
            struct mill_clause, epitem)->cr->valbuf, ch->sz);
    /* If there's a sender already waiting, let's resume it. */
    struct mill_clause *cl = mill_cont(
        mill_list_begin(&ch->sender.clauses), struct mill_clause, epitem);
    if(cl) {
        memcpy(dst, cl->val, ch->sz);
        mill_resume(cl->cr, cl->idx);
        mill_list_erase(&ch->sender.clauses, &cl->epitem);
        return;
    }
    /* Get the value from the buffer. */
    if(ch->items) {
        memcpy(dst, ((char*)(ch + 1)) + (ch->first * ch->sz), ch->sz);
        ch->first = (ch->first + 1) % ch->bufsz;
        --ch->items;
        return;
    }
    /* If the buffer is empty, the channel must have been done-with. */
    assert(ch->done);
    memcpy(dst, ((char*)(ch + 1)) + (ch->bufsz * ch->sz), ch->sz);
}

static void mill_choose_clean(struct mill_choose *uc, int exclude) {
    struct mill_slist_item *it;
    struct mill_clause *cl;
    for(it = mill_slist_begin(&uc->clauses); it; it = mill_slist_next(it)) {
        cl = mill_cont(it, struct mill_clause, chitem);
        if(cl->idx == exclude)
            continue;
        mill_list_erase(&cl->ep->clauses, &cl->epitem);
    }
}

int mill_choose_wait(void) {
    struct mill_choose *uc = &mill_running->u_choose;
    /* If there are clauses that are immediately available
       randomly choose one of them. */
    if(uc->available > 0) {
        int chosen = random() % (uc->available);
        struct mill_slist_item *it;
        struct mill_clause *cl;
        for(it = mill_slist_begin(&uc->clauses); it; it = mill_slist_next(it)) {
            cl = mill_cont(it, struct mill_clause, chitem);
            if(!cl->available)
                continue;
            if(!chosen)
                break;
            --chosen;
        }
        if(cl->ep->type == MILL_SENDER)
            mill_enqueue(mill_getchan(cl->ep), cl->val);
        else
            mill_dequeue(mill_getchan(cl->ep));
        mill_choose_clean(uc, -1);
        return cl->idx;
    }
    /* If not so but there's an 'otherwise' clause we can go straight to it. */
    else if(uc->othws) {
        mill_choose_clean(uc, -1);
        return -1;
    }

    /* In all other cases block and wait for an available channel. */
    int res = mill_suspend();
    mill_choose_clean(uc, res);
    return res;
}

void *mill_choose_val(void) {
    return mill_valbuf_get(&mill_running->valbuf);
}

void mill_chs(chan ch, void *val, size_t sz, const char *current) {
    mill_trace(current, "chr(<%d>)", (int)ch->debug.id);
    mill_choose_init_(current);
    mill_running->state = MILL_CHS;
    struct mill_clause cl;
    mill_choose_out(&cl, ch, val, sz, 0);
    mill_choose_wait();
}

void *mill_chr(chan ch, size_t sz, const char *current) {
    mill_trace(current, "chr(<%d>)", (int)ch->debug.id);
    mill_running->state = MILL_CHR;
    mill_choose_init_(current);
    struct mill_clause cl;
    mill_choose_in(&cl, ch, sz, 0);
    mill_choose_wait();
    return mill_choose_val();
}

void mill_chdone(chan ch, void *val, size_t sz, const char *current) {
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
        memcpy(mill_valbuf_alloc(&cl->cr->valbuf, ch->sz), val, ch->sz);
        mill_resume(cl->cr, cl->idx);
        mill_list_erase(&ch->receiver.clauses, &cl->epitem);
    }
}

