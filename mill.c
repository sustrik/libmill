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

#include "mill.h"

#include <assert.h>
#include <fcntl.h>
#include <poll.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

/******************************************************************************/
/*  Utilities                                                                 */
/******************************************************************************/

/*  Takes a pointer to a member variable and computes pointer to the structure
    that contains it. 'type' is type of the structure, not the member. */
#define mill_cont(ptr, type, member) \
    (ptr ? ((type*) (((char*) ptr) - offsetof(type, member))) : NULL)

/* Current time. Millisecond precision. */
static uint64_t mill_now() {
    struct timeval tv;
    int rc = gettimeofday(&tv, NULL);
    assert(rc == 0);
    return ((uint64_t)tv.tv_sec) * 1000 + (((uint64_t)tv.tv_usec) / 1000);
}

/******************************************************************************/
/*  Coroutines                                                                */
/******************************************************************************/

#define STACK_SIZE 16384

struct mill_cr {
    struct mill_cr *next;
    jmp_buf ctx;

    /* Following variables are used as a state for the choose statement being
       executed in the coroutine at the moment. */
    struct mill_clause *clist;
    void *othws;

    /* When this coroutine is blocked in choose and a different coroutine
       unblocks it, it copies the label from the clause that caused the
       resumption of execution to this field. */
    void *label;

    /* When coroutine is on hold, the time when it should resume execution. */
    uint64_t expiry;
};

/* Fake coroutine corresponding to the main thread of execution. */
static struct mill_cr main_cr = {NULL};

/* The queue of coroutines ready to run. The first one is currently running. */
static struct mill_cr *first_cr = &main_cr;
static struct mill_cr *last_cr = &main_cr;

/* Linked list of all coroutines on hold. The list is ordered.
   First coroutine to be resumed comes first and so on. */
static struct mill_cr *onhold = NULL;

/* Pollset used for waiting for file descriptors. */
static int wait_size = 0;
static int wait_capacity = 0;
static struct pollfd *wait_fds = NULL;
static struct mill_cr **wait_crs = NULL;

/* Removes current coroutine from the queue and returns it to the caller. */
static struct mill_cr *suspend() {
    struct mill_cr *cr = first_cr;
    first_cr = first_cr->next;
    if(!first_cr)
        last_cr = NULL;
    cr->next = NULL;
    return cr;
}

/* Schedules preiously suspended coroutine for execution. */
static void resume(struct mill_cr *cr) {
    cr->next = NULL;
    if(last_cr)
        last_cr->next = cr;
    else
        first_cr = cr;
    last_cr = cr;
}

/* Switch to a different coroutine. */
static void ctxswitch(void) {
    /* If there's a coroutine ready to be executed go for it. Otherwise,
       we are going to wait for onhold coroutines and external events. */
    if(first_cr)
        longjmp(first_cr->ctx, 1);

    /* The execution would block. Let's panic. */
    assert(onhold || wait_size);

    while(1) {
        /* Compute the time till next expired hold. */
        int timeout = -1;
        if(onhold) {
            uint64_t nw = mill_now();
            timeout = nw >= onhold->expiry ? 0 : onhold->expiry - nw;
        }

        /* Wait for events. */
        int rc = poll(wait_fds, wait_size, timeout);
        assert(rc >= 0);

        /* Resume all onhold coroutines that have expired. */
        if(onhold) {
            uint64_t nw = mill_now();
		    while(onhold && (onhold->expiry <= nw)) {
                struct mill_cr *cr = onhold;
                onhold = cr->next;
                resume(cr);
		    }
        }

        /* Resume coroutines waiting for file descriptors. */
        int i;
        for(i = 0; i != wait_size && rc; ++i) {
            if(wait_fds[i].revents) {
                resume(wait_crs[i]);
                wait_fds[i] = wait_fds[wait_size - 1];
                wait_crs[i] = wait_crs[wait_size - 1];
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
	longjmp(first_cr->ctx, 1);
}

/* The intial part of go(). Allocates the stack for the new coroutine. */
void *mill_go_prologue() {
    if(setjmp(first_cr->ctx))
        return NULL;
    char *ptr = malloc(STACK_SIZE);
    assert(ptr);
    struct mill_cr *cr =
        (struct mill_cr*)(ptr + STACK_SIZE - sizeof(struct mill_cr));
    cr->next = first_cr;
    cr->clist = NULL;
    cr->othws = NULL;
    cr->label = NULL;
    cr->expiry = 0;
    first_cr = cr;
    return (void*)cr;
}

/* The final part of go(). Cleans up when the coroutine is finished. */
void mill_go_epilogue(void) {
    struct mill_cr *cr = suspend();
    char *ptr = ((char*)(cr + 1)) - STACK_SIZE;
    free(ptr);
    ctxswitch();    
}

/* Move the current coroutine to the end of the queue.
   Pass control to the new head of the queue. */
void yield(void) {
    if(first_cr == last_cr)
        return;
    if(setjmp(first_cr->ctx))
        return;
    resume(suspend());
    ctxswitch();
}

/* hold current coroutine for a specified time interval. */
static void hold(unsigned long ms) {
    /* No point in waiting. However, let's give other coroutines a chance. */
    if(ms <= 0) {
        yield();
        return;
    }

    /* Save the current state. */
    if(setjmp(first_cr->ctx))
        return;
    
    /* Move the coroutine into the right place in the ordered list
       of onhold coroutines. */
    struct mill_cr *cr = suspend();
    cr->expiry = mill_now() + ms;
    struct mill_cr **it = &onhold;
    while(*it && (*it)->expiry <= cr->expiry)
        it = &((*it)->next);
    cr->next = *it;
    *it = cr;

    /* Pass control to a different coroutine. */
    ctxswitch();
}

/* Wait for events from a file descriptor. */
static void mill_wait(int fd, short events) {
    /* Grow the pollset as needed. */
    if(wait_size == wait_capacity) {
        wait_capacity = wait_capacity ? wait_capacity * 2 : 64;
        wait_fds = realloc(wait_fds, wait_capacity * sizeof(struct pollfd));
        wait_crs = realloc(wait_crs, wait_capacity * sizeof(struct mill_cr*));
    }

    /* Add the new file descriptor to the pollset. */
    wait_fds[wait_size].fd = fd;
    wait_fds[wait_size].events = events;
    wait_fds[wait_size].revents = 0;
    wait_crs[wait_size] = first_cr;
    ++wait_size;

    /* Save the current state and pass control to a different coroutine. */
    if(setjmp(first_cr->ctx))
        return;
    suspend();
    ctxswitch();
}

/******************************************************************************/
/*  Channels                                                                  */
/******************************************************************************/

/* Channel endpoint. */
struct mill_ep {
    enum {SENDER, RECEIVER} type;
    struct mill_clause *first_clause;
    struct mill_clause *last_clause;
    struct mill_ep *next;
};

/* Channel. */
struct chan {
    size_t sz;
    struct mill_ep sender;
    struct mill_ep receiver;
    int refcount;
    size_t bufsz;
    size_t items;
    size_t first;
};

static chan mill_getchan(struct mill_ep *ep) {
    switch(ep->type) {
    case SENDER:
        return mill_cont(ep, struct chan, sender);
    case RECEIVER:
        return mill_cont(ep, struct chan, receiver);
    default:
        assert(0);
    }
}

static struct mill_ep *mill_getpeer(struct mill_ep *ep) {
    switch(ep->type) {
    case SENDER:
        return &mill_cont(ep, struct chan, sender)->receiver;
    case RECEIVER:
        return &mill_cont(ep, struct chan, receiver)->sender;
    default:
        assert(0);
    }
}

static void mill_addclause(struct mill_ep *ep, struct mill_clause *clause) {
    if(!ep->last_clause) {
        assert(!ep->first_clause);
        clause->prev = NULL;
        clause->next = NULL;
        ep->first_clause = clause;
        ep->last_clause = clause;
        return;
    }
    ep->last_clause->next = clause;
    clause->prev = ep->last_clause;
    clause->next = NULL;
    ep->last_clause = clause;
}

static void mill_rmclause(struct mill_ep *ep, struct mill_clause *clause) {
    if(clause->prev)
        clause->prev->next = clause->next;
    else
        ep->first_clause = clause->next;
    if(clause->next)
        clause->next->prev = clause->prev;
    else
        ep->last_clause = clause->prev;
}

static int mill_enqueue(chan ch, void *val) {
    if(ch->items >= ch->bufsz)
        return 0;
    size_t pos = (ch->first + ch->items) % ch->bufsz;
    memcpy(((char*)(ch + 1)) + (pos * ch->sz) , val, ch->sz);
    ++ch->items;
    return 1;
}

static int mill_dequeue(chan ch, void *val) {
    if(!ch->items)
        return 0;
    memcpy(val, ((char*)(ch + 1)) + (ch->first * ch->sz), ch->sz);
    ch->first = (ch->first + 1) % ch->bufsz;
    --ch->items;
    return 1;
}

chan mill_chmake(size_t sz, size_t bufsz) {
    struct chan *ch = (struct chan*)malloc(sizeof(struct chan) + (sz * bufsz));
    assert(ch);
    ch->sz = sz;
    ch->sender.type = SENDER;
    ch->sender.first_clause = NULL;
    ch->sender.last_clause = NULL;
    ch->sender.next = NULL;
    ch->receiver.type = RECEIVER;
    ch->receiver.first_clause = NULL;
    ch->receiver.last_clause = NULL;
    ch->receiver.next = NULL;
    ch->refcount = 1;
    ch->bufsz = bufsz;
    ch->items = 0;
    ch->first = 0;
    return ch;
}

chan chdup(chan ch) {
    ++ch->refcount;
    return ch;
}

void mill_chs(chan ch, void *val, size_t sz) {
    /* Soft type checking. */
    assert (ch->sz == sz);

    /* If there's a receiver already waiting, we can just unblock it. */
    if(ch->receiver.first_clause) { 
        memcpy(ch->receiver.first_clause->val, val, sz);
        ch->receiver.first_clause->cr->label = ch->receiver.first_clause->label;
        resume(ch->receiver.first_clause->cr);
        mill_rmclause(&ch->receiver, ch->receiver.first_clause);
        return;
    }

    /* Write the message to the buffer. */
    if(mill_enqueue(ch, val))
       return;

    /* Otherwise we are going to yield till the receiver arrives. */
    if(setjmp(first_cr->ctx))
        return;
    struct mill_clause clause;
    clause.cr = suspend();
    clause.ep = &ch->sender;
    clause.val = val;
    clause.next_clause = NULL;
    mill_addclause(&ch->sender, &clause);

    /* Pass control to a different coroutine. */
    ctxswitch();
}

void *mill_chr(chan ch, void *val, size_t sz) {
    /* Soft type checking. */
    assert (ch->sz == sz);

    /* If there's a sender already waiting, we can just unblock it. */
    if(ch->sender.first_clause) {
        memcpy(val, ch->sender.first_clause->val, sz);
        ch->sender.first_clause->cr->label = ch->sender.first_clause->label;
        resume(ch->sender.first_clause->cr);
        mill_rmclause(&ch->sender, ch->sender.first_clause);
        return val;
    }

    /* Get a message from the buffer. */
    if(mill_dequeue(ch, val))
        return val;

    /* Otherwise we are going to yield till the sender arrives. */
    if(setjmp(first_cr->ctx))
        return val;
    struct mill_clause clause;
    clause.cr = suspend();
    clause.ep = &ch->receiver;
    clause.val = val;
    clause.next_clause = NULL;
    mill_addclause(&ch->receiver, &clause);

    /* Pass control to a different coroutine. */
    ctxswitch();
    /* NOTREACHED */
    return NULL;
}

void chclose(chan ch) {
    assert(ch->refcount >= 1);
    --ch->refcount;
    if(!ch->refcount) {
        assert(!ch->sender.first_clause);
        assert(!ch->receiver.first_clause);
        free(ch);
    }
}

void mill_choose_in(struct mill_clause *clause,
      chan ch, void *val, size_t sz, void *label) {
    /* Soft type checking. */
    assert (ch->sz == sz);

    /* Fill in the clause entry. */
    clause->cr = first_cr;
    clause->ep = &ch->receiver;
    clause->val = val;
    clause->label = label;
    clause->next_clause = first_cr->clist;
    first_cr->clist = clause;

    /* Add the clause to the channel's list of waiting clauses. */
    mill_addclause(&ch->receiver, clause);
}

void mill_choose_out(struct mill_clause *clause,
      chan ch, void *val, size_t sz, void *label) {
    /* Soft type checking. */
    assert (ch->sz == sz);

    /* Fill in the clause entry. */
    clause->cr = first_cr;
    clause->ep = &ch->sender;
    clause->val = val;
    clause->next_clause = first_cr->clist;
    clause->label = label;
    first_cr->clist = clause;

    /* Add the clause to the channel's list of waiting clauses. */
    mill_addclause(&ch->sender, clause);
}

void mill_choose_otherwise(void *label) {
    /* Check for multiple 'otherwise' cases. */
    assert(!first_cr->othws);
    first_cr->othws = label;
}

static int mill_isavailable(struct mill_ep *ep) {
    if(mill_getpeer(ep)->first_clause)
        return 1;
    chan ch = mill_getchan(ep);
    if(ep->type == SENDER) {
        if(ch->items < ch->bufsz)
            return 1;
    }
    else {
        if(ch->items)
            return 1;
    }
    return 0;
}

void *mill_choose_wait(void) {
    /* Find out wheter there are any channels that are already available. */
    int available = 0;
    struct mill_clause *it = first_cr->clist;
    while(it) {
        if(mill_isavailable(it->ep))
            ++available;
        it = it->next_clause;
    }
    
    /* If so, choose a random one. */
    if(available > 0) {
        int chosen = random() % available;
        void *res = NULL;
        it = first_cr->clist;
        while(it) {
            struct mill_ep *this_ep = it->ep;
            struct mill_ep *peer_ep = mill_getpeer(this_ep);
            chan ch = mill_getchan(this_ep);
            if(mill_isavailable(it->ep)) {
                if(!chosen) {
                    if(this_ep->type == SENDER) {
                        if(peer_ep->first_clause) {
                            memcpy(peer_ep->first_clause->val, it->val,
                                mill_getchan(it->ep)->sz);
                            peer_ep->first_clause->cr->label =
                                peer_ep->first_clause->label;
                            resume(peer_ep->first_clause->cr);
                            mill_rmclause(peer_ep, peer_ep->first_clause);
                        }
                        else {
                            mill_enqueue(ch, it->val);
                        }
                    }
                    else {
                        if(peer_ep->first_clause) {
                            memcpy(it->val, peer_ep->first_clause->val,
                                mill_getchan(it->ep)->sz);
                            peer_ep->first_clause->cr->label =
                                peer_ep->first_clause->label;
                            resume(peer_ep->first_clause->cr);
                            mill_rmclause(peer_ep, peer_ep->first_clause);
                        }
                        else {
                            mill_dequeue(ch, it->val);
                        }
                    }
                    res = it->label;
                    break;
                }
                --chosen;
            }
            it = it->next_clause;
        }
        assert(res);
        first_cr->label = res;
        goto cleanup;
    }

    /* If not so and there's an 'otherwise' clause we can go straight to it. */
    if(first_cr->othws) {
        first_cr->label = first_cr->othws;
        goto cleanup;
    }

    /* In all other cases block and wait for an available channel. */
    if(!setjmp(first_cr->ctx)) {
        suspend();
        ctxswitch();
    }
   
    /* Clean-up the clause lists in queried channels. */
    cleanup:
    for(it = first_cr->clist; it; it = it->next_clause)
        mill_rmclause(it->ep, it);
    first_cr->clist = NULL;
    first_cr->othws = NULL;
    return first_cr->label;
}

/******************************************************************************/
/*  Library                                                                   */
/******************************************************************************/

void msleep(unsigned int sec) {
    hold(sec * 1000);
}

void musleep(unsigned long us) {
    uint64_t ms = us / 1000;
    if(us % 1000)
        ++ms;
    hold(ms);
}

int msocket(int family, int type, int protocol) {
    int s = socket(family, type, protocol);
    if(s == -1)
        return -1;
    int opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    int rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    assert(rc != -1);
    return s;
}

int mconnect(int s, const struct sockaddr *addr, socklen_t addrlen) {
    int rc = connect(s, addr, addrlen);
    if(rc == 0)
        return 0;
    assert(rc == -1);
    if(errno != EINPROGRESS)
        return -1;
    mill_wait(s, POLLOUT);
    /* TODO: Handle errors. */
    return 0;
}

int maccept(int s, struct sockaddr *addr, socklen_t *addrlen) {
    while(1) {
        int newsock = accept(s, addr, addrlen);
        if (newsock >= 0) {
            int opt = fcntl(newsock, F_GETFL, 0);
            if (opt == -1)
                opt = 0;
            int rc = fcntl(newsock, F_SETFL, opt | O_NONBLOCK);
            assert(rc != -1);
            return newsock;
        }
        assert(newsock == -1);
        if(errno != EAGAIN && errno != EWOULDBLOCK)
            return -1;
        mill_wait(s, POLLIN);
    }
}

ssize_t msend(int s, const void *buf, size_t len, int flags) {
    char *pos = (char*)buf;
    size_t remaining = len;
    while(remaining) {
        ssize_t sz = send(s, pos, remaining, flags);
        if(sz == -1) {
            if(errno != EAGAIN && errno != EWOULDBLOCK)
                return -1;
            mill_wait(s, POLLOUT);
            continue;
        }
        pos += sz;
        remaining -= sz;
    }
    return len;
}

ssize_t mrecv(int s, void *buf, size_t len, int flags) {
    char *pos = (char*)buf;
    size_t remaining = len;
    while(remaining) {
        ssize_t sz = recv(s, pos, remaining, flags);
        if(sz == 0)
            return len - remaining;
        if(sz == -1) {
            if(errno != EAGAIN && errno != EWOULDBLOCK)
                return -1;
            mill_wait(s, POLLIN);
            continue;
        }
        pos += sz;
        remaining -= sz;
    }
    return len;
}

