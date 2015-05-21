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

#include "libmill.h"
#include "list.h"
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

/* Size of stack for new coroutines. In bytes. */
#define MILL_STACK_SIZE 16384

/* Maximum size of an item in a channel that we can handle without
   extra memory allocation. */
#define MILL_MAXINLINECHVALSIZE 128

/* Maximum number of unused cached stacks. */
#define MILL_MAX_CACHED_STACKS 64

volatile int mill_unoptimisable1 = 1;
volatile void *mill_unoptimisable2 = NULL;

/* This structure keeps the state of a 'choose' operation. */
struct mill_chstate {
    /* List of clauses in the 'choose' statement. */
    struct mill_clause *clauses;

    /* 1 if there is 'otherwise' clause. 0 if there is not. */
    int othws;

    /* Number of clauses that are immediately available. */
    int available;

    /* When coroutine is blocked in choose and a different coroutine
       unblocks it, it copies the index from the clause that caused the
       resumption of execution to this field, so that 'choose' statement
       can find out what exactly have happened. */
    int idx;
};

static void mill_chstate_init(struct mill_chstate *chstate) {
    chstate->clauses = NULL;
    chstate->othws = 0;
    chstate->available = 0;
    chstate->idx = -2;
}

enum mill_state {
    MILL_YIELD,
    MILL_MSLEEP,
    MILL_FDWAIT,
    MILL_CHR,
    MILL_CHS,
    MILL_CHOOSE
};

/* The coroutine. This structure is held on the top of the coroutine's stack. */
struct mill_cr {
    /* The list of all coroutines. */
    struct mill_list_item all_crs_item;

    /* Unique ID of the coroutine. */
    int id;

    /* Filename and line number of where the coroutine was created. */
    const char *created;

    /* Filename and line number of where the current operation was
       invoked from. */
    const char *current;

    /* Status of the coroutine. Used for debugging purposes. */
    enum mill_state state;

    /* A linked list the coroutine is in. It can be either the list of
       coroutines ready for execution, list of sleeping coroutines or NULL. */
    struct mill_cr *next;

    /* Stored coroutine context while it is not executing. */
    struct mill_ctx ctx;

    /* State for the choose operation that's underway in this coroutine. */
    struct mill_chstate chstate;

    /* Place to store the received value when doing choose. 'ptr' doesn't get
       deallocated for the main coroutine, however, we don't care as the
       process is terminating at that point anyway. */
    struct {
        void *ptr;
        size_t capacity;
        char buf[MILL_MAXINLINECHVALSIZE];
    } val;

    /* When coroutine is sleeping, the time when it should resume execution. */
    uint64_t expiry;

    /* When doing fdwait() this field is used to transfer the result to the
       blocked coroutine. */
    int fdwres;

    /* Coroutine-local storage. */
    void *cls;
};

/* ID to be assigned to next launched coroutine. */
static int next_cr_id = 1;

/* Fake coroutine corresponding to the main thread of execution. */
static struct mill_cr main_cr = {NULL};

/* List of all coroutines. Used for debugging purposes. */
struct mill_list all_crs = {&main_cr.all_crs_item, &main_cr.all_crs_item};

/* The queue of coroutines ready to run. The first one is currently running. */
static struct mill_cr *first_cr = &main_cr;
static struct mill_cr *last_cr = &main_cr;

/* Linked list of all sleeping coroutines. The list is ordered.
   First coroutine to be resume comes first and so on. */
static struct mill_cr *sleeping = NULL;

/* Pollset used for waiting for file descriptors. */
static int wait_size = 0;
static int wait_capacity = 0;
static struct pollfd *wait_fds = NULL;
struct mill_fdwitem {
    struct mill_cr *in;
    struct mill_cr *out;
};
static struct mill_fdwitem *wait_items = NULL;

/* A stack of unused coroutine stacks. This allows for extra-fast allocation
   of a new stack. The FIFO nature of this structure minimises cache misses. */
static int num_cached_crs = 0;
static struct mill_cr *cached_crs = NULL;

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
    if(!sleeping && !wait_size)
        mill_panic("global hang-up");

    while(1) {
        /* Compute the time till next expired sleeping coroutine. */
        int timeout = -1;
        if(sleeping) {
            uint64_t nw = mill_now();
            timeout = nw >= sleeping->expiry ? 0 : sleeping->expiry - nw;
        }

        /* Wait for events. */
        int rc = poll(wait_fds, wait_size, timeout);
        assert(rc >= 0);

        /* Resume all sleeping coroutines that have expired. */
        if(sleeping) {
            uint64_t nw = mill_now();
		    while(sleeping && (sleeping->expiry <= nw)) {
                struct mill_cr *cr = sleeping;
                sleeping = cr->next;
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
    struct mill_cr *cr;
    if(cached_crs) {
        cr = cached_crs;
        cached_crs = cached_crs->next;
        --num_cached_crs;
    }
    else {
        char *ptr = malloc(MILL_STACK_SIZE);
        assert(ptr);
        cr = (struct mill_cr*)(ptr + MILL_STACK_SIZE - sizeof(struct mill_cr));
    }
    mill_list_insert(&all_crs, &cr->all_crs_item, mill_list_end(&all_crs));
    cr->id = next_cr_id;
    cr->created = created;
    cr->current = NULL;
    ++next_cr_id;
    cr->state = MILL_YIELD;
    cr->next = first_cr;
    mill_chstate_init(&cr->chstate);
    cr->val.ptr = NULL;
    cr->val.capacity = MILL_MAXINLINECHVALSIZE;
    cr->expiry = 0;
    cr->fdwres = 0;
    cr->cls = NULL;
    first_cr = cr;
    return (void*)cr;
}

/* The final part of go(). Cleans up when the coroutine is finished. */
void mill_go_epilogue(void) {
    struct mill_cr *cr = mill_suspend();
    if(cr->val.ptr) {
        free(cr->val.ptr);
        cr->val.ptr = NULL;
    }
    mill_list_erase(&all_crs, &cr->all_crs_item);
    if(num_cached_crs >= MILL_MAX_CACHED_STACKS) {
        char *ptr = ((char*)(cr + 1)) - MILL_STACK_SIZE;
        free(ptr);
    }
    else {
        cr->next = cached_crs;
        cached_crs = cr;
        ++num_cached_crs;
    }
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
void mill_msleep(unsigned long ms, const char *current) {
    /* No point in waiting. However, let's give other coroutines a chance. */
    if(ms <= 0) {
        yield();
        return;
    }

    /* Save the current state. */
    if(mill_setjmp(&first_cr->ctx))
        return;
    
    /* Move the coroutine into the right place in the ordered list
       of sleeping coroutines. */
    struct mill_cr *cr = mill_suspend();
    cr->state = MILL_MSLEEP;
    cr->expiry = mill_now() + ms;
    cr->current = current;
    struct mill_cr **it = &sleeping;
    while(*it && (*it)->expiry <= cr->expiry)
        it = &((*it)->next);
    cr->next = *it;
    *it = cr;

    /* Pass control to a different coroutine. */
    mill_ctxswitch();
}

/* Waiting for events from a file descriptor. */
int mill_fdwait(int fd, int events, const char *current) {
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

/* Channel endpoint. */
struct mill_ep {
    /* Thanks to this flag we can cast from ep pointer to chan pointer. */
    enum {MILL_SENDER, MILL_RECEIVER} type;
    /* List of clauses waiting for this endpoint. */
    struct mill_list clauses;
};

/* Channel. */
struct chan {
    /* List of all channels. Used for debugging purposes. */
    struct mill_list_item all_chans_item;

    /* Channel ID. */
    int id;

    /* Filename and line number of where the channel was created. */
    const char *created;

    /* The size of the elements stored in the channel, in bytes. */
    size_t sz;
    /* Channel holds two lists, the list of clauses waiting to send and list
       of clauses waiting to receive. */
    struct mill_ep sender;
    struct mill_ep receiver;
    /* Number of open handles to this channel. */
    int refcount;
    /* 1 is chdone() was already called. 0 otherwise. */
    int done;

    /* The message buffer directly follows the chan structure. 'bufsz' specifies
       the maximum capacity of the buffer. 'items' is the number of messages
       currently in the buffer. 'first' is the index of the next message to
       receive from the buffer. */
    size_t bufsz;
    size_t items;
    size_t first;
};

/* ID to be assigned to the next created channel. */
static int mill_next_chan_id = 1;

/* List of all channels. Used for debugging purposes. */
static struct mill_list all_chans = {0};

/* This structure represents a single clause in a choose statement.
   Similarly, both chs() and chr() each create a single clause. */
struct mill_clause {
    /* Member of list of clauses waiting for a channel endpoint. */
    struct mill_list_item item;
    /* Linked list of clauses in the choose statement. */
    struct mill_clause *next_clause;
    /* The coroutine which created the clause. */
    struct mill_cr *cr;
    /* Channel endpoint the clause is waiting for. */
    struct mill_ep *ep;
    /* For out clauses, pointer to the value to send. For in clauses it is
       either point to the value to receive to or NULL. In the latter case
       the value should be received into coroutines in buffer. */
    void *val;
    /* The index to jump to when the clause is executed. */
    int idx;
    /* If 0, there's no peer waiting for the clause at the moment.
       If 1, there is one. */
    int available;
};

MILL_CT_ASSERT(MILL_CLAUSELEN == sizeof(struct mill_clause));

static chan mill_getchan(struct mill_ep *ep) {
    switch(ep->type) {
    case MILL_SENDER:
        return mill_cont(ep, struct chan, sender);
    case MILL_RECEIVER:
        return mill_cont(ep, struct chan, receiver);
    default:
        assert(0);
    }
}

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
            mill_list_begin(&ch->receiver.clauses), struct mill_clause, item);
        void *dst = cl->val;
        if(!dst)
            dst = mill_getvalbuf(cl->cr, ch->sz);
        memcpy(dst, val, ch->sz);
        cl->cr->chstate.idx = cl->idx;
        mill_resume(cl->cr);
        mill_list_erase(&ch->receiver.clauses, &cl->item);
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
            struct mill_clause, item)->cr, ch->sz);

    /* If there's a sender already waiting, let's resume it. */
    struct mill_clause *cl = mill_cont(
        mill_list_begin(&ch->sender.clauses), struct mill_clause, item);
    if(cl) {
        memcpy(dst, cl->val, ch->sz);
        cl->cr->chstate.idx = cl->idx;
        mill_resume(cl->cr);
        mill_list_erase(&ch->sender.clauses, &cl->item);
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
    /* We are allocating 1 additional element after the channel buffer to
       store the done-with value. It can't be stored in the regular buffer
       because that would mean chdone() would block when buffer is full. */
    struct chan *ch = (struct chan*)malloc(sizeof(struct chan) +
        (sz * (bufsz + 1)));
    assert(ch);
    mill_list_insert(&all_chans, &ch->all_chans_item,
        mill_list_end(&all_chans));
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
    return ch;
}

chan chdup(chan ch) {
    ++ch->refcount;
    return ch;
}

void mill_chs(chan ch, void *val, size_t sz, const char *current) {
    if(ch->done)
        mill_panic("send to done-with channel");
    if(ch->sz != sz)
        mill_panic("send of a type not matching the channel");

    if(mill_enqueue(ch, val))
        return;

    /* If there's no free space in the buffer we are going to yield
       till the receiver arrives. */
    if(mill_setjmp(&first_cr->ctx)) {
        first_cr->chstate.clauses = NULL;
        return;
    }
    struct mill_clause cl;
    cl.cr = mill_suspend();
    cl.cr->state = MILL_CHS;
    cl.ep = &ch->sender;
    cl.val = val;
    cl.next_clause = NULL;
    mill_list_insert(&ch->sender.clauses, &cl.item,
        mill_list_end(&ch->sender.clauses));
    cl.cr->chstate.clauses = &cl;
    cl.cr->current = current;

    /* Pass control to a different coroutine. */
    mill_ctxswitch();
}

void *mill_chr(chan ch, void *val, size_t sz, const char *current) {
    if(ch->sz != sz)
        mill_panic("receive of a type not matching the channel");

    if(mill_dequeue(ch, val)) {
        first_cr->chstate.clauses = NULL;
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
    cl.next_clause = NULL;
    mill_list_insert(&ch->receiver.clauses, &cl.item,
        mill_list_end(&ch->receiver.clauses));
    cl.cr->chstate.clauses = &cl;
    cl.cr->current = current;

    /* Pass control to a different coroutine. */
    mill_ctxswitch();
    /* Unreachable, but let's make XCode happy. */
    return NULL;
}

void mill_chdone(chan ch, void *val, size_t sz) {
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
            mill_list_begin(&ch->receiver.clauses), struct mill_clause, item);
        void *dst = cl->val;
        if(!dst)
            dst = mill_getvalbuf(cl->cr, ch->sz);
        memcpy(dst, val, ch->sz);
        cl->cr->chstate.idx = cl->idx;
        mill_resume(cl->cr);
        mill_list_erase(&ch->receiver.clauses, &cl->item);
    }
}

void chclose(chan ch) {
    assert(ch->refcount >= 1);
    --ch->refcount;
    if(!ch->refcount) {
        assert(mill_list_empty(&ch->sender.clauses));
        assert(mill_list_empty(&ch->receiver.clauses));
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
    cl->next_clause = first_cr->chstate.clauses;
    first_cr->chstate.clauses = clause;

    /* Add the clause to the channel's list of waiting clauses. */
    mill_list_insert(&ch->receiver.clauses, &cl->item,
        mill_list_end(&ch->receiver.clauses));
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
    cl->next_clause = first_cr->chstate.clauses;
    cl->available = available;
    cl->idx = idx;
    first_cr->chstate.clauses = cl;

    /* Add the clause to the channel's list of waiting clauses. */
    mill_list_insert(&ch->sender.clauses, &cl->item,
        mill_list_end(&ch->sender.clauses));
}

void mill_choose_otherwise(void) {
    if(first_cr->chstate.othws != 0)
        mill_panic("multiple 'otherwise' clauses in a choose statement");
    first_cr->chstate.othws = 1;
}

int mill_choose_wait(const char *current) {
    struct mill_chstate *chstate = &first_cr->chstate;
    int res = -1;
    struct mill_clause *it;
    
    /* If there are clauses that are immediately available,
       randomly choose one of them. */
    if(chstate->available > 0) {
        int chosen = random() % (chstate->available);
        it = chstate->clauses;
        while(it) {
            if(it->available) {
                if(!chosen) {
                    int ok = it->ep->type == MILL_SENDER ?
                        mill_enqueue(mill_getchan(it->ep), it->val) :
                        mill_dequeue(mill_getchan(it->ep), NULL);
                    assert(ok);
                    res = it->idx;
                    break;
                }
                --chosen;
            }
            it = it->next_clause;
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
    for(it = chstate->clauses; it; it = it->next_clause)
        mill_list_erase(&it->ep->clauses, &it->item);
    mill_chstate_init(chstate);

    assert(res >= -1);
    return res;
}

void *mill_choose_val(void) {
    return first_cr->val.ptr ? first_cr->val.ptr : first_cr->val.buf;
}

/******************************************************************************/
/*  Debugging                                                                 */
/******************************************************************************/

void goredump(void) {
    char buf[256];
    fprintf(stderr,
        "\nCOROUTINE   state                  current              created\n");
    fprintf(stderr,
        "---------------------------------------------------------------\n");
    struct mill_list_item *it;
    for(it = mill_list_begin(&all_crs);
          it != mill_list_end(&all_crs);
          it = mill_list_next(&all_crs, it)) {
        struct mill_cr *cr = mill_cont(it, struct mill_cr, all_crs_item);
        switch(cr->state) {
        case MILL_YIELD:
            sprintf(buf, "%s", first_cr == cr ? "running" : "yield()");
            break;
        case MILL_MSLEEP:
            sprintf(buf, "msleep()");
            break;
        case MILL_FDWAIT:
            sprintf(buf, "fdwait(%d)", -1);
            break;
        case MILL_CHR:
            sprintf(buf, "chr(%d)", mill_getchan(cr->chstate.clauses->ep)->id);
            break;
        case MILL_CHS:
            sprintf(buf, "chs(%d)", mill_getchan(cr->chstate.clauses->ep)->id);
            break;
        case MILL_CHOOSE:
            {
		        int pos = 0;
                pos += sprintf(&buf[pos], "choose(");
		        struct mill_clause *cl = cr->chstate.clauses;
		        int first = 1;
		        while(cl) { // TODO: List the clauses in reverse order.
		            if(first)
		                first = 0;
		            else
		                pos += sprintf(&buf[pos], ",");
		            pos += sprintf(&buf[pos], "%d", mill_getchan(cl->ep)->id);
		            cl = cl->next_clause;
		        }
		        sprintf(&buf[pos], ")");
            }
            break;
        default:
            assert(0);
        }
        fprintf(stderr, "%s (%06d) %-22s %-20s %s\n",
            cr == first_cr ? "=>" : "  ",
            cr->id,
            buf,
            cr == first_cr ? "---" : cr->current,
            cr->created ? cr->created : "<main>");
    }
    fprintf(stderr,"\n");
    if(mill_list_empty(&all_chans))
        return;
    fprintf(stderr,
        "CHANNEL  msgs/max    senders/receivers      refs  done  created\n");
    fprintf(stderr,
        "---------------------------------------------------------------\n");
    for(it = mill_list_begin(&all_chans);
          it != mill_list_end(&all_chans);
          it = mill_list_next(&all_chans, it)) {
        struct chan *ch = mill_cont(it, struct chan, all_chans_item);
        sprintf(buf, "%d/%d",
            (int)ch->items,
            (int)ch->bufsz);
        fprintf(stderr, "(%06d) %-11s ",
            ch->id,
            buf);
        int pos;
        struct mill_list *clauselist;
        if(!mill_list_empty(&ch->sender.clauses)) {
            pos = sprintf(buf, "s:");
            clauselist = &ch->sender.clauses;
        }
        else if(!mill_list_empty(&ch->receiver.clauses)) {
            pos = sprintf(buf, "r:");
            clauselist = &ch->receiver.clauses;
        }
        else {
            sprintf(buf, " ");
            clauselist = NULL;
        }
        struct mill_clause *cl = NULL;
        if(clauselist)
            cl = mill_cont(mill_list_begin(clauselist),
                struct mill_clause, item);
        int first = 1;
        while(cl && &cl->item != mill_list_end(clauselist)) {
            if(first)
                first = 0;
            else
                pos += sprintf(&buf[pos], ",");
            pos += sprintf(&buf[pos], "%d", (int)cl->cr->id);
            cl = mill_cont(mill_list_next(clauselist, &cl->item),
                struct mill_clause, item);
        }
        fprintf(stderr, "%-22s %-5d %-5s %s\n",
            buf,
            (int)ch->refcount,
            ch->done ? "yes" : "no",
            ch->created);            
    }
    fprintf(stderr,"\n");
}

