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

#include "debug.h"
#include "libmill.h"
#include "list.h"
#include "model.h"
#include "slist.h"
#include "stack.h"
#include "utils.h"

#include <assert.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/******************************************************************************/
/*  Coroutines                                                                */
/******************************************************************************/

volatile int mill_unoptimisable1 = 1;
volatile void *mill_unoptimisable2 = NULL;

static void mill_chstate_init(struct mill_chstate *self) {
    mill_slist_init(&self->clauses);
    self->othws = 0;
    self->available = 0;
    self->idx = -2;
}

/* Removes current coroutine from the queue and returns it to the caller. */
static struct mill_cr *mill_suspend() {
    struct mill_cr *cr = first_cr;
    first_cr = first_cr->next;
    if(!first_cr)
        last_cr = NULL;
    cr->next = NULL;
    return cr;
}

/* Schedules preiously suspended coroutine for execution. */
static void mill_resume(struct mill_cr *cr) {
    cr->next = NULL;
    if(last_cr)
        last_cr->next = cr;
    else
        first_cr = cr;
    last_cr = cr;
    cr->state = MILL_YIELD;
}

/* Switch to a different coroutine. */
static void mill_ctxswitch(void) {
    /* If there's a coroutine ready to be executed go for it. Otherwise,
       we are going to wait for sleeping coroutines and for external events. */
    if(first_cr)
        mill_jmp(&first_cr->ctx);

    /* The execution of the entore process would block. Let's panic. */
    if(mill_slist_empty(&sleeping) && !wait_size)
        mill_panic("global hang-up");

    while(1) {
        /* Compute the time till next expired sleeping coroutine. */
        int timeout = -1;
        if(!mill_slist_empty(&sleeping)) {
            uint64_t nw = mill_now();
            uint64_t expiry = mill_cont(mill_slist_begin(&sleeping),
                struct mill_cr, sleeping_item)->expiry;
            timeout = nw >= expiry ? 0 : expiry - nw;
        }

        /* Wait for events. */
        int rc = poll(wait_fds, wait_size, timeout);
        assert(rc >= 0);

        /* Resume all sleeping coroutines that have expired. */
        if(!mill_slist_empty(&sleeping)) {
            uint64_t nw = mill_now();
            while(!mill_slist_empty(&sleeping)) {
                struct mill_cr *cr = mill_cont(mill_slist_begin(&sleeping),
                    struct mill_cr, sleeping_item);
                if(cr->expiry > nw)
                    break;
                mill_slist_pop(&sleeping);
                mill_resume(cr);
            }
        }

        /* Resume coroutines waiting for file descriptors. */
        int i;
        for(i = 0; i != wait_size && rc; ++i) {
            /* Set the result values. */
            if(wait_fds[i].revents & POLLIN) {
                assert(wait_items[i].in);
                wait_items[i].in->fdwres |= FDW_IN;
            }
            if(wait_fds[i].revents & POLLOUT) {
                assert(wait_items[i].out);
                wait_items[i].out->fdwres |= FDW_OUT;
            }
            if(wait_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                if(wait_items[i].in)
                    wait_items[i].in->fdwres |= FDW_ERR;
                if(wait_items[i].out)
                    wait_items[i].out->fdwres |= FDW_ERR;
            }
            /* Unblock waiting coroutines. */
            if(wait_items[i].in && wait_items[i].in->fdwres) {
                mill_resume(wait_items[i].in);
                wait_fds[i].events &= ~POLLIN;
                wait_items[i].in = NULL;
            }
            if(wait_items[i].out && wait_items[i].out->fdwres) {
                mill_resume(wait_items[i].out);
                wait_fds[i].events &= ~POLLOUT;
                wait_items[i].out = NULL;
            }
            /* If nobody is waiting for the fd remove it from the pollset. */
            if(!wait_fds[i].events) {
                assert(!wait_items[i].in && !wait_items[i].out);
                wait_fds[i] = wait_fds[wait_size - 1];
                wait_items[i] = wait_items[wait_size - 1];
                --wait_size;
                --rc;
            }
        }

        /* If no coroutine was resumed, do the poll again. This should not
           happen in theory, but let's be ready for the case when timers
           are not precise. */
        if(first_cr)
            break;
    }

	/* Pass control to a resumed coroutine. */
	mill_jmp(&first_cr->ctx);
}

/* The intial part of go(). Allocates the stack for the new coroutine. */
void *mill_go_prologue(const char *created) {
    if(mill_setjmp(&first_cr->ctx))
        return NULL;
    struct mill_cr *cr = ((struct mill_cr*)mill_allocstack()) - 1;
    mill_list_insert(&all_crs, &cr->all_crs_item, NULL);
    cr->id = next_cr_id;
    cr->created = created;
    cr->current = NULL;
    ++next_cr_id;
    cr->state = MILL_YIELD;
    mill_chstate_init(&cr->chstate);
    cr->val.ptr = NULL;
    cr->val.capacity = MILL_MAXINLINECHVALSIZE;
    cr->expiry = 0;
    cr->fdwres = 0;
    cr->cls = NULL;

    /* Move the current coroutine to the end of the queue. */
    mill_resume(mill_suspend());    

    /* Put the new coroutine to the beginning of the queue. */
    cr->next = first_cr;
    first_cr = cr;

    mill_trace(created, "go()");

    return (void*)cr;
}

/* The final part of go(). Cleans up when the coroutine is finished. */
void mill_go_epilogue(void) {

    mill_trace(NULL, "go() done");

    struct mill_cr *cr = mill_suspend();
    if(cr->val.ptr) {
        free(cr->val.ptr);
        cr->val.ptr = NULL;

        /* Ensure that debug functions are available if a single go()
           statement is present in the user's code. */
        mill_preserve_debug();
    }
    mill_list_erase(&all_crs, &cr->all_crs_item);
    mill_freestack(cr + 1);
    mill_ctxswitch();
}

/* Move the current coroutine to the end of the queue.
   Pass control to the new head of the queue. */
void mill_yield(const char *current) {
    if(first_cr == last_cr)
        return;
    if(mill_setjmp(&first_cr->ctx))
        return;
    struct mill_cr *cr = mill_suspend();
    cr->current = current;
    mill_resume(cr);
    mill_ctxswitch();
}

/* Pause current coroutine for a specified time interval. */
void mill_msleep(long ms, const char *current) {
    /* No point in waiting. However, let's give other coroutines a chance. */
    if(ms <= 0) {
        yield();
        return;
    }

    /* Save the current state. */
    if(mill_setjmp(&first_cr->ctx))
        return;

    struct mill_cr *cr = mill_suspend();
    cr->state = MILL_MSLEEP;
    cr->expiry = mill_now() + ms;
    cr->current = current;
    
    /* Move the coroutine into the right place in the ordered list
       of sleeping coroutines. */
    struct mill_slist_item *it = mill_slist_begin(&sleeping);
    if(!it) {
        mill_slist_push(&sleeping, &cr->sleeping_item);
    }
    else {
       while(it) {
           struct mill_cr *next = mill_cont(mill_slist_next(it),
               struct mill_cr, sleeping_item);
           if(!next || next->expiry > cr->expiry) {
               mill_slist_insert(&sleeping, &cr->sleeping_item, it);
               break;
           }
           it = mill_slist_next(it);
       }
    }

    /* Pass control to a different coroutine. */
    mill_ctxswitch();
}

/* Waiting for events from a file descriptor. */
int mill_fdwait(int fd, int events, long timeout, const char *current) {
    /* Find the fd in the pollset. TODO: This is O(n)! */
    int i;
    for(i = 0; i != wait_size; ++i) {
        if(wait_fds[i].fd == fd)
            break;
    }

    /* Grow the pollset as needed. */
    if(i == wait_size) { 
		if(wait_size == wait_capacity) {
		    wait_capacity = wait_capacity ? wait_capacity * 2 : 64;
		    wait_fds = realloc(wait_fds,
		        wait_capacity * sizeof(struct pollfd));
		    wait_items = realloc(wait_items,
		        wait_capacity * sizeof(struct mill_fdwitem));
		}
        ++wait_size;
        wait_fds[i].fd = fd;
        wait_fds[i].events = 0;
        wait_fds[i].revents = 0;
        wait_items[i].in = NULL;
        wait_items[i].out = NULL;
    }

    /* Register the waiting coroutine in the pollset. */
    if(events & FDW_IN) {
        if(wait_items[i].in)
            mill_panic(
                "multiple coroutines waiting for a single file descriptor");
        wait_fds[i].events |= POLLIN;
        wait_items[i].in = first_cr;
    }
    if(events & FDW_OUT) {
        if(wait_items[i].out)
            mill_panic(
                "multiple coroutines waiting for a single file descriptor");
        wait_fds[i].events |= POLLOUT;
        wait_items[i].out = first_cr;
    }

    /* Save the current state and pass control to a different coroutine. */
    if(!mill_setjmp(&first_cr->ctx)) {
        struct mill_cr *cr = mill_suspend();
        cr->state = MILL_FDWAIT;
        cr->current = current;
        mill_ctxswitch();
    }

    /* Return the value sent by the polling coroutine. */
    int res = first_cr->fdwres;
    first_cr->fdwres = 0;
    return res;
}

void *cls(void) {
    return first_cr->cls;
}

void setcls(void *val) {
    first_cr->cls = val;
}

/******************************************************************************/
/*  Channels                                                                  */
/******************************************************************************/

MILL_CT_ASSERT(MILL_CLAUSELEN == sizeof(struct mill_clause));

/* Returns pointer to the coroutinr'd in-buffer. The buffer is reallocated
   as needed to accommodate 'sz' bytes of content. */
static void *mill_getvalbuf(struct mill_cr *cr, size_t sz) {
    size_t capacity = cr->val.capacity ?
          cr->val.capacity :
          MILL_MAXINLINECHVALSIZE;
    /* If there's enough capacity for the type available, return either
       dynamically allocated buffer, if present, or the static buffer. */
    if(capacity >= sz)
        return cr->val.ptr ? cr->val.ptr : cr->val.buf;
    /* Allocate or grow the dyncamic buffer to accommodate the type. */
    cr->val.ptr = realloc(cr->val.ptr, sz);
    assert(cr->val.ptr);
    cr->val.capacity = sz;
    return cr->val.ptr;
}

/* Add new item to the channel buffer. */
static int mill_enqueue(chan ch, void *val) {
    /* If there's a receiver already waiting, let's resume it. */
    if(!mill_list_empty(&ch->receiver.clauses)) {
        struct mill_clause *cl = mill_cont(
            mill_list_begin(&ch->receiver.clauses), struct mill_clause, epitem);
        void *dst = cl->val;
        if(!dst)
            dst = mill_getvalbuf(cl->cr, ch->sz);
        memcpy(dst, val, ch->sz);
        cl->cr->chstate.idx = cl->idx;
        mill_resume(cl->cr);
        mill_list_erase(&ch->receiver.clauses, &cl->epitem);
        return 1;
    }
    /* The buffer is full. */
    if(ch->items >= ch->bufsz)
        return 0;
    /* Write the value to the buffer. */
    size_t pos = (ch->first + ch->items) % ch->bufsz;
    memcpy(((char*)(ch + 1)) + (pos * ch->sz) , val, ch->sz);
    ++ch->items;
    return 1;
}

/* Pop one value from the channel buffer. */
static int mill_dequeue(chan ch, void *val) {
    void *dst = val;
    if(!dst)
        dst = mill_getvalbuf(mill_cont(mill_list_begin(&ch->receiver.clauses),
            struct mill_clause, epitem)->cr, ch->sz);

    /* If there's a sender already waiting, let's resume it. */
    struct mill_clause *cl = mill_cont(
        mill_list_begin(&ch->sender.clauses), struct mill_clause, epitem);
    if(cl) {
        memcpy(dst, cl->val, ch->sz);
        cl->cr->chstate.idx = cl->idx;
        mill_resume(cl->cr);
        mill_list_erase(&ch->sender.clauses, &cl->epitem);
        return 1;
    }

    /* The buffer is empty. */
    if(!ch->items) {
        if(!ch->done)
            return 0;
        /* Receiving from a closed channel yields done-with value. */
        memcpy(dst, ((char*)(ch + 1)) + (ch->bufsz * ch->sz), ch->sz);
        return 1;
    }
    /* Get the value from the buffer. */
    memcpy(dst, ((char*)(ch + 1)) + (ch->first * ch->sz), ch->sz);
    ch->first = (ch->first + 1) % ch->bufsz;
    --ch->items;
    return 1;
}

chan mill_chmake(size_t sz, size_t bufsz, const char *created) {
    /* If there's at least one channel created in the user's code
       we want the debug functions to get into the binary. */
    mill_preserve_debug();

    /* We are allocating 1 additional element after the channel buffer to
       store the done-with value. It can't be stored in the regular buffer
       because that would mean chdone() would block when buffer is full. */
    struct mill_chan *ch = (struct mill_chan*)malloc(sizeof(struct mill_chan) +
        (sz * (bufsz + 1)));
    assert(ch);
    mill_list_insert(&all_chans, &ch->all_chans_item, NULL);
    ch->id = mill_next_chan_id;
    ++mill_next_chan_id;
    ch->created = created;
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
    mill_trace(created, "CH%06d=chmake(%d)", (int)ch->id, (int)bufsz);
    return ch;
}

chan chdup(chan ch) {
    ++ch->refcount;
    return ch;
}

void mill_chs(chan ch, void *val, size_t sz, const char *current) {
    mill_trace(current, "chs(CH%06d)", (int)ch->id);

    if(ch->done)
        mill_panic("send to done-with channel");
    if(ch->sz != sz)
        mill_panic("send of a type not matching the channel");

    if(mill_enqueue(ch, val))
        return;

    /* If there's no free space in the buffer we are going to yield
       till the receiver arrives. */
    if(mill_setjmp(&first_cr->ctx)) {
        mill_slist_init(&first_cr->chstate.clauses);
        return;
    }
    struct mill_clause cl;
    cl.cr = mill_suspend();
    cl.cr->state = MILL_CHS;
    cl.ep = &ch->sender;
    cl.val = val;
    mill_list_insert(&ch->sender.clauses, &cl.epitem, NULL);
    mill_slist_push_back(&cl.cr->chstate.clauses, &cl.chitem);
    cl.cr->current = current;

    /* Pass control to a different coroutine. */
    mill_ctxswitch();
}

void *mill_chr(chan ch, void *val, size_t sz, const char *current) {
    mill_trace(current, "chr(CH%06d)", (int)ch->id);

    if(ch->sz != sz)
        mill_panic("receive of a type not matching the channel");

    if(mill_dequeue(ch, val)) {
        mill_slist_init(&first_cr->chstate.clauses);
        return val;
    }

    /* If there's no message in the buffer we are going to yield
       till the sender arrives. */
    if(mill_setjmp(&first_cr->ctx))
        return val;
    struct mill_clause cl;
    cl.cr = mill_suspend();
    cl.cr->state = MILL_CHR;
    cl.ep = &ch->receiver;
    cl.val = val;
    mill_list_insert(&ch->receiver.clauses, &cl.epitem, NULL);
    mill_slist_push_back(&cl.cr->chstate.clauses, &cl.chitem);
    cl.cr->current = current;

    /* Pass control to a different coroutine. */
    mill_ctxswitch();
    /* Unreachable, but let's make XCode happy. */
    return NULL;
}

void mill_chdone(chan ch, void *val, size_t sz, const char *current) {
    mill_trace(current, "chdone(CH%06d)", (int)ch->id);

    if(ch->done)
        mill_panic("chdone on already done-with channel");
    if(ch->sz != sz)
        mill_panic("send of a type not matching the channel");

    /* Panic if there are other senders on the same channel. */
    if(!mill_list_empty(&ch->sender.clauses))
        mill_panic("send to done-with channel");

    /* Put the channel into done-with mode. */
    ch->done = 1;
    memcpy(((char*)(ch + 1)) + (ch->bufsz * ch->sz) , val, ch->sz);

    /* Resume all the receivers currently waiting on the channel. */
    while(!mill_list_empty(&ch->receiver.clauses)) {
        struct mill_clause *cl = mill_cont(
            mill_list_begin(&ch->receiver.clauses), struct mill_clause, epitem);
        void *dst = cl->val;
        if(!dst)
            dst = mill_getvalbuf(cl->cr, ch->sz);
        memcpy(dst, val, ch->sz);
        cl->cr->chstate.idx = cl->idx;
        mill_resume(cl->cr);
        mill_list_erase(&ch->receiver.clauses, &cl->epitem);
    }
}

void mill_chclose(chan ch, const char *current) {
    mill_trace(current, "chclose(CH%06d)", (int)ch->id);
    assert(ch->refcount >= 1);
    --ch->refcount;
    if(!ch->refcount) {
        mill_list_term(&ch->sender.clauses);
        mill_list_term(&ch->receiver.clauses);
        mill_list_erase(&all_chans, &ch->all_chans_item);
        free(ch);
    }
}

void mill_choose_in(void *clause, chan ch, size_t sz, int idx) {
    if(ch->sz != sz)
        mill_panic("receive of a type not matching the channel");

    /* Find out whether the clause is immediately available. */
    int available = ch->done || !mill_list_empty(&ch->sender.clauses) ||
        ch->items ? 1 : 0;
    if(available)
        ++first_cr->chstate.available;

    /* If there are available clauses don't bother with non-available ones. */
    if(!available && first_cr->chstate.available)
        return;

    /* Fill in the clause entry. */
    struct mill_clause *cl = (struct mill_clause*) clause;
    cl->cr = first_cr;
    cl->ep = &ch->receiver;
    cl->val = NULL;
    cl->idx = idx;
    cl->available = available;
    mill_slist_push_back(&first_cr->chstate.clauses, &cl->chitem);

    /* Add the clause to the channel's list of waiting clauses. */
    mill_list_insert(&ch->receiver.clauses, &cl->epitem, NULL);
}

void mill_choose_out(void *clause, chan ch, void *val, size_t sz, int idx) {
    if(ch->done)
        mill_panic("send to done-with channel");
    if(ch->sz != sz)
        mill_panic("send of a type not matching the channel");

    /* Find out whether the clause is immediately available. */
    int available = !mill_list_empty(&ch->receiver.clauses) ||
        ch->items < ch->bufsz ? 1 : 0;
    if(available)
        ++first_cr->chstate.available;

    /* If there are available clauses don't bother with non-available ones. */
    if(!available && first_cr->chstate.available)
        return;

    /* Fill in the clause entry. */
    struct mill_clause *cl = (struct mill_clause*) clause;
    cl->cr = first_cr;
    cl->ep = &ch->sender;
    cl->val = val;
    cl->available = available;
    cl->idx = idx;
    mill_slist_push_back(&first_cr->chstate.clauses, &cl->chitem);

    /* Add the clause to the channel's list of waiting clauses. */
    mill_list_insert(&ch->sender.clauses, &cl->epitem, NULL);
}

void mill_choose_otherwise(void) {
    if(first_cr->chstate.othws != 0)
        mill_panic("multiple 'otherwise' clauses in a choose statement");
    first_cr->chstate.othws = 1;
}

int mill_choose_wait(const char *current) {
    mill_trace(current, "choose{}");

    struct mill_chstate *chstate = &first_cr->chstate;
    int res = -1;
    struct mill_slist_item *it;
    
    /* If there are clauses that are immediately available,
       randomly choose one of them. */
    if(chstate->available > 0) {
        int chosen = random() % (chstate->available);
        for (it = mill_slist_begin(&chstate->clauses); it != NULL;
              it = mill_slist_next(it)) {
            struct mill_clause *cl = mill_cont(it, struct mill_clause, chitem);
            if(cl->available) {
                if(!chosen) {
                    int ok = cl->ep->type == MILL_SENDER ?
                        mill_enqueue(mill_getchan(cl->ep), cl->val) :
                        mill_dequeue(mill_getchan(cl->ep), NULL);
                    assert(ok);
                    res = cl->idx;
                    break;
                }
                --chosen;
            }
        }
        goto cleanup;
    }

    /* If not so but there's an 'otherwise' clause we can go straight to it. */
    if(chstate->othws) {
        res = -1;
        goto cleanup;
    }

    /* In all other cases block and wait for an available channel. */
    if(!mill_setjmp(&first_cr->ctx)) {
        struct mill_cr *cr = mill_suspend();
        cr->state = MILL_CHOOSE;
        cr->current = current;
        mill_ctxswitch();
    }
    /* Get the result clause as set up by the coroutine that just unblocked
       this choose statement. */
    res = chstate->idx;
   
    /* Clean-up the clause lists in queried channels. */
    cleanup:
    for(it = mill_slist_begin(&chstate->clauses); it;
          it = mill_slist_next(it)) {
        struct mill_clause *cl = mill_cont(it, struct mill_clause, chitem);
        mill_list_erase(&cl->ep->clauses, &cl->epitem);
    }
    mill_chstate_init(chstate);

    assert(res >= -1);
    return res;
}

void *mill_choose_val(void) {
    return first_cr->val.ptr ? first_cr->val.ptr : first_cr->val.buf;
}

