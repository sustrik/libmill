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
#include <sys/types.h>
#include <sys/socket.h>

/******************************************************************************/
/*  Coroutines                                                                */
/******************************************************************************/

void *mill_go_prologue(void);
void mill_go_epilogue(void);

#define go(fn) \
    do {\
        void *mill_sp = mill_go_prologue();\
        if(mill_sp) {\
            volatile int mill_unoptimisable = 1;\
            int mill_anchor[mill_unoptimisable];\
            char mill_filler[(char*)&mill_anchor - (char*)(mill_sp)];\
            fn;\
            mill_go_epilogue();\
        }\
    } while(0)

void yield(void);

/******************************************************************************/
/*  Channels                                                                  */
/******************************************************************************/

typedef struct chan *chan;

chan chmake(void);
void chaddref(chan ch);
void chs(chan ch, void *val);
void *chr(chan ch);
void chclose(chan ch);

#define mill_concat(x,y) x##y

#define choose \
    {\
        struct ep *mill_chlist = NULL;\
        int mill_blocking = 1;\
        int mill_res = -2;\
        while(1) {\
            {\
                if(mill_res != -2) {\
                    if(0)

#define mill_in(chan, name, idx) \
                        break;\
                    }\
                    goto mill_concat(mill_label, idx);\
                }\
            }\
            {\
                void *name = NULL;\
                mill_chlist = mill_choose_in(mill_chlist, chan, idx, &name);\
                if(0) {\
                    mill_concat(mill_label, idx):\
                    if(mill_res == idx) {\
                        mill_concat(mill_dummylabel, idx)

#define in(chan, name) mill_in(chan, name, __COUNTER__)

#define mill_out(chan, val, idx) \
                        break;\
                    }\
                    goto mill_concat(mill_label, idx);\
                }\
            }\
            {\
                void *mill_outval##idx = (val);\
                mill_chlist = mill_choose_out(mill_chlist, chan, idx,\
                    &mill_outval##idx);\
                if(0) {\
                    mill_concat(mill_label, idx):\
                    if(mill_res == idx) {\
                        mill_concat(mill_dummylabel, idx)

#define out(chan, val) mill_out(chan, val, __COUNTER__)

#define otherwise \
                        break;\
                    }\
                    goto mill_concat(mill_label, idx);\
                }\
            }\
            {\
                mill_blocking = 0;\
                if(0) {\
                    mill_concat(mill_label, idx):\
                    if(1) {\
                        mill_concat(mill_dummylabel, idx)

#define end \
                        break;\
                    }\
                }\
            }\
            mill_res = mill_choose_wait(mill_blocking, mill_chlist);\
        }

struct ep *mill_choose_in(struct ep *chlist, chan ch, int idx, void **val);
struct ep *mill_choose_out(struct ep *chlist, chan ch, int idx, void **val);
int mill_choose_wait(int blocking, struct ep *chlist);

/******************************************************************************/
/*  Library                                                                   */
/******************************************************************************/

void msleep(unsigned int sec);
void musleep(unsigned long us);

int msocket(int family, int type, int protocol);
int mconnect(int s, const struct sockaddr *addr, socklen_t addrlen);
int maccept(int s, struct sockaddr *address, socklen_t *addrlen);
ssize_t msend(int s, const void *buf, size_t len, int flags);
ssize_t msendto(int s, const void *buf, size_t len, int flags,
    const struct sockaddr *addr, socklen_t addrlen);
ssize_t msendmsg(int s, const struct msghdr *msg, int flags);
ssize_t mrecv(int s, void *buf, size_t len, int flags);
ssize_t mrecvfrom(int s, void *buf, size_t len, int flags,
    struct sockaddr *addr, socklen_t *addrlen);
ssize_t mrecvmsg(int s, struct msghdr *msg, int flags);

#endif

