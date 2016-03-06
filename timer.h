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

#ifndef MILL_TIMER_INCLUDED
#define MILL_TIMER_INCLUDED

#include <stdint.h>

#include "list.h"

struct mill_timer;

typedef void (*mill_timer_callback)(struct mill_timer *timer);

struct mill_timer {
    /* Item in the global list of all timers. */
    struct mill_list_item item;
    /* The deadline when the timer expires. -1 if the timer is not active. */
    int64_t expiry;
    /* Callback invoked when timer expires. Pfui Teufel! */
    mill_timer_callback callback;
};

/* Test wheather the timer is active. */
#define mill_timer_enabled(tm)  ((tm)->expiry >= 0)

/* Add a timer for the running coroutine. */
void mill_timer_add(struct mill_timer *timer, int64_t deadline,
    mill_timer_callback callback);

/* Remove the timer associated with the running coroutine. */
void mill_timer_rm(struct mill_timer *timer);

/* Number of milliseconds till the next timer expires.
   If there are no timers returns -1. */
int mill_timer_next(void);

/* Resumes all coroutines whose timers have already expired.
   Returns zero if no coroutine was resumed, 1 otherwise. */
int mill_timer_fire(void);

/* Called after fork in the child process to deactivate all the timers
   inherited from the parent. */
void mill_timer_postfork(void);

#endif

