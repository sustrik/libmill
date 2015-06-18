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

#ifndef MILL_CR_INCLUDED
#define MILL_CR_INCLUDED

#include "debug.h"
#include "list.h"
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

struct mill_ready {
    struct mill_slist_item item;
};

/* This structure covers fdwait and msleep operations. */
struct mill_fdwait {
    /* Item in the global list of timers. */
    struct mill_list_item item;
    /* The timepoint when the timer expires. */
    uint64_t expiry;
};

/* This structure covers chr, chs and choose operations. */
struct mill_choose {
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

    struct mill_ready u_ready;
    struct mill_fdwait u_fdwait;
    struct mill_choose u_choose;

    /* Stored coroutine context while it is not executing. */
    struct mill_ctx ctx;

    /* Place to temporarily store the value received from a channel. */
    struct mill_valbuf valbuf;

    /* Argument to resume() call being passed to the blocked suspend() call. */
    int result;

    /* Coroutine-local storage. */
    void *cls;

    /* Debugging info. */
    struct mill_debug_cr debug;
};

/* Fake coroutine corresponding to the main thread of execution. */
extern struct mill_cr mill_main;

/* The coroutine that is running at the moment. */
extern struct mill_cr *mill_running;

/* Suspend running coroutine. Move to executing different coroutines. Once
   someone resumes this coroutine using mill_resume function 'result' argument
   of that function will be returned. */
int mill_suspend(void);

/* Schedules preiously suspended coroutine for execution. Keep in mind that
   it doesn't immediately run it, just puts it into the queue of ready
   coroutines. */
void mill_resume(struct mill_cr *cr, int result);

#endif
