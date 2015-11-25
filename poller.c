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

#include <stdint.h>

#include "libmill.h"
#include "list.h"

/* Global linked list of all timers. The list is ordered.
   First timer to be resume comes first and so on. */
static struct mill_list mill_timers = {0};

/* Pause current coroutine for a specified time interval. */
void mill_msleep(int64_t deadline, const char *current) {
    mill_fdwait(-1, 0, deadline, current);
}

/* Include the poll-mechanism-specific stuff. */

/* User overloads. */
#if defined MILL_EPOLL
#include "epoll.inc"
#elif defined MILL_KQUEUE
#include "kqueue.inc"
#elif defined MILL_POLL
#include "poll.inc"
/* Defaults. */
#elif 0 && defined __linux__
#include "epoll.inc"
#else
#include "poll.inc"
#endif
