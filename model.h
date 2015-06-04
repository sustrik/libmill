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

#ifndef MILL_MODEL_INCLUDED
#define MILL_MODEL_INCLUDED

#include "debug.h"
#include "model.h"
#include "list.h"
#include "poller.h"
#include "slist.h"
#include "utils.h"
#include "valbuf.h"

#include <stdint.h>

enum mill_state {
    MILL_READY,
    MILL_MSLEEP,
    MILL_FDWAIT,
    MILL_CHR,
    MILL_CHS,
    MILL_CHOOSE
};

struct mill_timer {
    /* Item of the global list of timers. */
    struct mill_slist_item item;
    /* The timepoint when the timer expires. */
    uint64_t expiry;
};

struct mill_poll {
};

/* This structure keeps the state of a 'choose' operation. */
struct mill_chstate {
    /* List of clauses in the 'choose' statement. */
    struct mill_slist clauses;

    /* 1 if there is 'otherwise' clause. 0 if there is not. */
    int othws;

    /* Number of clauses that are immediately available. */
    int available;
};

/* The coroutine. This structure is held on the top of the coroutine's stack. */
struct mill_cr {
    /* Status of the coroutine. Used for debugging purposes. */
    enum mill_state state;

    /* List of coroutines ready to be executed. */
    struct mill_slist_item item;

    /* Used when the coroutine is sleeping. */
    struct mill_timer sleeper;

    /* Used when the coroutine is waiting for a file descriptor. */
    struct mill_poll poller;

    /* Stored coroutine context while it is not executing. */
    struct mill_ctx ctx;

    /* State for the choose operation that's underway in this coroutine. */
    struct mill_chstate chstate;

    /* Place to store the received value when doing choose. */
    struct mill_valbuf valbuf;

    /* Argument to resume() call being passed to the blocked suspend() call. */
    int result;

    /* Coroutine-local storage. */
    void *cls;

    /* Debugging info. */
    struct mill_debug_cr debug;
};

/* Fake coroutine corresponding to the main thread of execution. */
extern struct mill_cr main_cr;

/* The coroutine that is running at the moment. */
extern struct mill_cr *mill_running;

/* Channel endpoint. */
struct mill_ep {
    /* Thanks to this flag we can cast from ep pointer to chan pointer. */
    enum {MILL_SENDER, MILL_RECEIVER} type;
    /* List of clauses waiting for this endpoint. */
    struct mill_list clauses;
};

/* Channel. */
struct mill_chan {

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
       be received from the buffer. */
    size_t bufsz;
    size_t items;
    size_t first;

    /* Debugging info. */
    struct mill_debug_chan debug;
};

/* This structure represents a single clause in a choose statement.
   Similarly, both chs() and chr() each create a single clause. */
struct mill_clause {
    /* Member of list of clauses waiting for a channel endpoint. */
    struct mill_list_item epitem;
    /* Linked list of clauses in the choose statement. */
    struct mill_slist_item chitem;
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

/* Returns pointer to the channel that contains specified endpoint. */
struct mill_chan *mill_getchan(struct mill_ep *ep);

#endif

