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

#ifndef MILL_H_INCLUDED
#define MILL_H_INCLUDED

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

/******************************************************************************/
/* Debugging support                                                          */
/******************************************************************************/

/* Calling this function turns coroutine tracking on. From that point on events
   such as coroutine creation, context switches et c. are reported to stdout. */
void trace(void);

/* goredump() dumps the current state of all coroutines in the program. */
#define goredump goredump_(__FILE__, __LINE__)
typedef void (*goredump_fn_)(void);
goredump_fn_ goredump_(const char *file, int name);

/******************************************************************************/
/* Coroutines                                                                 */
/******************************************************************************/

/* go(expression) evaluates the expression in a new coroutine. */

#define stringify_internal_(x) #x
#define stringify_(x) stringify_internal_(x)

#define set_sp_(ptr) \
    volatile int unoptimisable_ = 1;\
    int filler1_[unoptimisable_];\
    char filler2_[(char*)&filler1_ - (char*)(ptr)];

void *go_prologue_(const char *name);
void go_epilogue_();

#define go(fn) \
    {\
        void *sp = go_prologue_(\
            #fn " (" __FILE__ ":" stringify_(__LINE__) ")");\
        if(sp) {\
            set_sp_(sp);\
            fn;\
            go_epilogue_();\
        }\
    }

/* Yield CPU to a different coroutine. */
void yield(void);

/******************************************************************************/
/* Channels                                                                   */
/******************************************************************************/

/* A non-buffered channel. */
typedef struct chan *chan;

/* The value passed via the channels. The value isn't interpreted in any way
   by mill. */
typedef union {
    int i;
    uint32_t u32;
    void *ptr;
} chan_val;

chan chan_init(void);
void chan_send(chan ch, chan_val val);
chan_val chan_recv(chan ch);
void chan_close(chan ch);

/* This family of functions waits for multiple channels. If there are channels
   ready to be received from, it chooses one of them randomly. If there are not
   if either returns -1 (tryselect variants) or waits until there's one
   available (select variants). When the channel is selected it receives
   the value from the channel to the variable pointed to by first argument and
   returns the index of the channel in the channel array. Variants without
   -v suffix expect the array of channels to be passed as additional arguments:

     int res = chan_select(&val, ch1, ch2, ch3);

   If that case the result is the index of the channel in the channel list,
   not the index of the argument. So, if the above function returns 0 it means
   that ch1 was received from.
*/ 
#define chan_select(...) chan_select_(1, __VA_ARGS__, (chan)NULL);
#define chan_tryselect(...) chan_select_(0, __VA_ARGS__, (chan)NULL);
int chan_selectv(chan_val *val, chan *chans, int nchans);
int chan_tryselectv(chan_val *val, chan *chans, int nchans);

/* Internal function. Do not use directly. */
int chan_select_(int block, chan_val *val, ...);

/******************************************************************************/
/* Coroutine-friendly versions of system calls                                */
/******************************************************************************/

/* These functions correspond to their POSIX counterparts (without m- prefix).
   However, when blocked they yield CPU to other coroutines rather than blocking
   the entire thread of execution. */

void msleep(unsigned int seconds);
void musleep(unsigned int microseconds);

/* To avoid extra system calls, sockets passed to these functions are assumed
   to be in non-blocking mode. If they are not, the behaviour is undefined.
   To create sockets in non-blocking mode you can use msocket() convenience
   function. */
int msocket(int domain, int type, int protocol);
int mconnect(int s, const struct sockaddr *addr, socklen_t addrlen);
int maccept(int s, struct sockaddr *addr, socklen_t *addrlen);
ssize_t msend(int s, const void *buf, size_t len, int flags);
ssize_t mrecv(int s, void *buf, size_t len, int flags);

/******************************************************************************/
/* The library                                                                */
/******************************************************************************/

/* Sends an event to the returned channel after specified
   number of milliseconds. */
chan after(int ms);

#endif

