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

#define mill_assert(x, text) \
    do {\
        if (!(x)) {\
            fprintf(stderr, "%s (%s:%d)\n", text, \
                __FILE__, __LINE__);\
            fflush(stderr);\
            abort();\
        }\
    } while (0)

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

#define MILL_STACK_SIZE 16384

/* This structure keeps the state of a 'choose' operation. */
struct mill_chstate {
    /* List of clauses in the 'choose' statement. */
    struct mill_clause *clauses;

    /* Label pointing to the 'otherwise' clause. NULL if there is none. */
    void *othws;

    /* When coroutine is blocked in choose and a different coroutine
       unblocks it, it copies the label from the clause that caused the
       resumption of execution to this field, so that 'choose' statement
       can find out what exactly have happened. */
    void *label;
};

/* The coroutine. This structure is held on the top of the coroutine's stack. */
struct mill_cr {
    /* A linked list the coroutine is in. It can be either the list of
       coroutines ready for execution, list of sleeping coroutines or NULL. */
    struct mill_cr *next;

    /* Stored coroutine context while it is not executing. */
    jmp_buf ctx;

    /* Ongoing choose operation. */
    struct mill_chstate chstate;

    /* When coroutine is sleeping, the time when it should resume execution. */
    uint64_t expiry;
};

/* Fake coroutine corresponding to the main thread of execution. */
static struct mill_cr main_cr = {NULL};

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
static struct mill_cr **wait_crs = NULL;

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
}

/* Switch to a different coroutine. */
static void ctxswitch(void) {
    /* If there's a coroutine ready to be executed go for it. Otherwise,
       we are going to wait for sleeping coroutines and for external events. */
    if(first_cr)
        longjmp(first_cr->ctx, 1);

    /* The execution would block. Let's panic. */
    mill_assert(sleeping || wait_size, "There are no coroutines eligible for "
        "resumption. The process would block forever.");

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
            if(wait_fds[i].revents) {
                mill_resume(wait_crs[i]);
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
    char *ptr = malloc(MILL_STACK_SIZE);
    assert(ptr);
    struct mill_cr *cr =
        (struct mill_cr*)(ptr + MILL_STACK_SIZE - sizeof(struct mill_cr));
    cr->next = first_cr;
    cr->chstate.clauses = NULL;
    cr->chstate.othws = NULL;
    cr->chstate.label = NULL;
    cr->expiry = 0;
    first_cr = cr;
    return (void*)cr;
}

/* The final part of go(). Cleans up when the coroutine is finished. */
void mill_go_epilogue(void) {
    struct mill_cr *cr = mill_suspend();
    char *ptr = ((char*)(cr + 1)) - MILL_STACK_SIZE;
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
    mill_resume(mill_suspend());
    ctxswitch();
}

/* Pause current coroutine for a specified time interval. */
void msleep(unsigned long ms) {
    /* No point in waiting. However, let's give other coroutines a chance. */
    if(ms <= 0) {
        yield();
        return;
    }

    /* Save the current state. */
    if(setjmp(first_cr->ctx))
        return;
    
    /* Move the coroutine into the right place in the ordered list
       of sleeping coroutines. */
    struct mill_cr *cr = mill_suspend();
    cr->expiry = mill_now() + ms;
    struct mill_cr **it = &sleeping;
    while(*it && (*it)->expiry <= cr->expiry)
        it = &((*it)->next);
    cr->next = *it;
    *it = cr;

    /* Pass control to a different coroutine. */
    ctxswitch();
}

/* Start waiting for the events from a file descriptor. */
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
    mill_suspend();
    ctxswitch();
}

/******************************************************************************/
/*  Channels                                                                  */
/******************************************************************************/

/* Channel endpoint. */
struct mill_ep {
    /* Thanks to this flag we can cast from ep pointer to chan pointer. */
    enum {MILL_SENDER, MILL_RECEIVER} type;
    /* DOuble-linked list of clauses waiting for this endpoint. */
    struct mill_clause *first_clause;
    struct mill_clause *last_clause;
};

/* Channel. */
struct chan {
    /* The size of the elements stored in the channel, in bytes. */
    size_t sz;
    /* Channel holds two lists, the list of clauses waiting to send and list
       of clauses waiting to receive. */
    struct mill_ep sender;
    struct mill_ep receiver;
    /* Number of open handles to this channel. */
    int refcount;

    /* The message buffer directly follows the chan structure. 'bufsz' specifies
       the maximum capacity of the buffer. 'items' is the number of messages
       currently in the buffer. 'first' is the index of the next message to
       receive from the buffer. */
    size_t bufsz;
    size_t items;
    size_t first;
};

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

static struct mill_ep *mill_getpeer(struct mill_ep *ep) {
    switch(ep->type) {
    case MILL_SENDER:
        return &mill_cont(ep, struct chan, sender)->receiver;
    case MILL_RECEIVER:
        return &mill_cont(ep, struct chan, receiver)->sender;
    default:
        assert(0);
    }
}

/* Add the clause to the endpoint's list of waiting clauses. */
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

/* Remove the clause from the endpoint's list of waiting clauses. */
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

/* Add new item to the channel buffer. */
static int mill_enqueue(chan ch, void *val) {
    if(ch->items >= ch->bufsz)
        return 0;
    size_t pos = (ch->first + ch->items) % ch->bufsz;
    memcpy(((char*)(ch + 1)) + (pos * ch->sz) , val, ch->sz);
    ++ch->items;
    return 1;
}

/* Pop one value from the channel buffer. */
static int mill_dequeue(chan ch, void *val) {
    if(!ch->items)
        return 0;
    memcpy(val, ((char*)(ch + 1)) + (ch->first * ch->sz), ch->sz);
    ch->first = (ch->first + 1) % ch->bufsz;
    --ch->items;
    return 1;
}

static void mill_resume_receiver(struct mill_ep *ep, void *val) {
    memcpy(ep->first_clause->val, val, mill_getchan(ep)->sz);
    ep->first_clause->cr->chstate.label = ep->first_clause->label;
    mill_resume(ep->first_clause->cr);
    mill_rmclause(ep, ep->first_clause);
}

static void mill_resume_sender(struct mill_ep *ep, void *val) {
    memcpy(val, ep->first_clause->val, mill_getchan(ep)->sz);
    ep->first_clause->cr->chstate.label = ep->first_clause->label;
    mill_resume(ep->first_clause->cr);
    mill_rmclause(ep, ep->first_clause);
}

chan mill_chmake(size_t sz, size_t bufsz) {
    struct chan *ch = (struct chan*)malloc(sizeof(struct chan) + (sz * bufsz));
    assert(ch);
    ch->sz = sz;
    ch->sender.type = MILL_SENDER;
    ch->sender.first_clause = NULL;
    ch->sender.last_clause = NULL;
    ch->receiver.type = MILL_RECEIVER;
    ch->receiver.first_clause = NULL;
    ch->receiver.last_clause = NULL;
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
    mill_assert(ch->sz == sz,
        "Sending a value of incorrect type to a channel.");

    /* If there's a receiver already waiting, we can just unblock it. */
    if(ch->receiver.first_clause) {
        mill_resume_receiver(&ch->receiver, val);
        return;
    }

    /* Write the message to the buffer. */
    if(mill_enqueue(ch, val))
       return;

    /* If there's no free space in the buffer we are going to yield
       till the receiver arrives. */
    if(setjmp(first_cr->ctx))
        return;
    struct mill_clause clause;
    clause.cr = mill_suspend();
    clause.ep = &ch->sender;
    clause.val = val;
    clause.next_clause = NULL;
    mill_addclause(&ch->sender, &clause);

    /* Pass control to a different coroutine. */
    ctxswitch();
}

void *mill_chr(chan ch, void *val, size_t sz) {
    /* Soft type checking. */
    mill_assert(ch->sz == sz,
        "Receiving a value of incorrect type from a channel");

    /* If there's a sender already waiting, we can just unblock it. */
    if(ch->sender.first_clause) {
        mill_resume_sender(&ch->sender, val);
        return val;
    }

    /* Get a message from the buffer. */
    if(mill_dequeue(ch, val))
        return val;

    /* If there's no message in the buffer we are going to yield
       till the sender arrives. */
    if(setjmp(first_cr->ctx))
        return val;
    struct mill_clause clause;
    clause.cr = mill_suspend();
    clause.ep = &ch->receiver;
    clause.val = val;
    clause.next_clause = NULL;
    mill_addclause(&ch->receiver, &clause);

    /* Pass control to a different coroutine. */
    ctxswitch();
    /* Unreachable, but let's make XCode happy. */
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
    mill_assert(ch->sz == sz,
        "Receiving a value of incorrect type from a channel.");

    /* Fill in the clause entry. */
    clause->cr = first_cr;
    clause->ep = &ch->receiver;
    clause->val = val;
    clause->label = label;
    clause->next_clause = first_cr->chstate.clauses;
    first_cr->chstate.clauses = clause;

    /* Add the clause to the channel's list of waiting clauses. */
    mill_addclause(&ch->receiver, clause);
}

void mill_choose_out(struct mill_clause *clause,
      chan ch, void *val, size_t sz, void *label) {
    /* Soft type checking. */
    mill_assert(ch->sz == sz,
        "Sending a value of incorrect type to a channel.");

    /* Fill in the clause entry. */
    clause->cr = first_cr;
    clause->ep = &ch->sender;
    clause->val = val;
    clause->next_clause = first_cr->chstate.clauses;
    clause->label = label;
    first_cr->chstate.clauses = clause;

    /* Add the clause to the channel's list of waiting clauses. */
    mill_addclause(&ch->sender, clause);
}

void mill_choose_otherwise(void *label) {
    mill_assert(!first_cr->chstate.othws,
        "Multiple 'otherwise' clauses in a choose statement.");
    first_cr->chstate.othws = label;
}

static int mill_isavailable(struct mill_ep *ep) {
    if(mill_getpeer(ep)->first_clause)
        return 1;
    chan ch = mill_getchan(ep);
    if(ep->type == MILL_SENDER) {
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
    struct mill_chstate *chstate = &first_cr->chstate;

    /* Find out wheter there are any channels that are already available. */
    int available = 0;
    struct mill_clause *it = chstate->clauses;
    while(it) {
        if(mill_isavailable(it->ep))
            ++available;
        it = it->next_clause;
    }
    
    /* If so, choose a random one. */
    if(available > 0) {
        int chosen = random() % available;
        void *res = NULL;
        it = chstate->clauses;
        while(it) {
            struct mill_ep *peer_ep = mill_getpeer(it->ep);
            if(mill_isavailable(it->ep)) {
                if(!chosen) {
                    if(it->ep->type == MILL_SENDER) {
                        if(peer_ep->first_clause) {
                            mill_resume_receiver(peer_ep, it->val);
                        }
                        else {
                            mill_enqueue(mill_getchan(it->ep), it->val);
                        }
                    }
                    else {
                        if(peer_ep->first_clause) {
                            mill_resume_sender(peer_ep, it->val);
                        }
                        else {
                            mill_dequeue(mill_getchan(it->ep), it->val);
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
        chstate->label = res;
        goto cleanup;
    }

    /* If not so and there's an 'otherwise' clause we can go straight to it. */
    if(chstate->othws) {
        chstate->label = chstate->othws;
        goto cleanup;
    }

    /* In all other cases block and wait for an available channel. */
    if(!setjmp(first_cr->ctx)) {
        mill_suspend();
        ctxswitch();
    }
   
    /* Clean-up the clause lists in queried channels. */
    cleanup:
    for(it = chstate->clauses; it; it = it->next_clause)
        mill_rmclause(it->ep, it);
    chstate->clauses = NULL;
    chstate->othws = NULL;
    return chstate->label;
}

/******************************************************************************/
/*  Library                                                                   */
/******************************************************************************/

static void mill_after(chan ch, unsigned long ms) {
    msleep(ms);
    chs(ch, int, 0);
    chclose(ch);
}

chan after(unsigned long ms) {
    chan ch = chmake(int, 1);
    go(mill_after(chdup(ch), ms));
    return ch;
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

