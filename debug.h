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

#ifndef MILL_DEBUG_INCLUDED
#define MILL_DEBUG_INCLUDED

#include "list.h"
#include "utils.h"

struct mill_debug_cr {
    /* List of all coroutines. */
    struct mill_list_item item;
    /* Unique ID of the coroutine. */
    int id;
    /* File and line where the coroutine was launched. */
    const char *created;
    /* File and line where the current blocking operation was invoked from. */
    const char *current;
};

struct mill_debug_chan {
    /* List of all channels. */
    struct mill_list_item item;
    /* Unique ID of the channel. */
    int id;
    /* File and line where the channel was created. */
    const char *created;
};

/* Cause panic. */
void mill_panic(const char *text);

/* No-op, but ensures that debugging functions get compiled into the binary. */
void mill_preserve_debug(void);

/* (Un)register coroutines and channels with the debugging subsystem. */
void mill_register_cr(struct mill_debug_cr *cr, const char *created);
void mill_unregister_cr(struct mill_debug_cr *cr);
void mill_register_chan(struct mill_debug_chan *ch, const char *created);
void mill_unregister_chan(struct mill_debug_chan *ch);

/* While doing a blocking operation coroutine should register where
   the operation was invoked from. */
void mill_set_current(struct mill_debug_cr *cr, const char *current);

extern int mill_tracelevel;

/* Create a trace record. */
#define mill_trace if(mill_slow(mill_tracelevel)) mill_trace_
void mill_trace_(const char *location, const char *format, ...);

/* Returns 1 if there are any coroutines running, 0 otherwise. */
int mill_hascrs(void);

#endif
