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

#include <alloca.h>
#include <errno.h>
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

struct mill_cp;
struct mill_ep;

/* This structure represents a single clause in a choose statement.
   Similarly, both chs() and chr() each create a single clause. */
struct mill_clause {
    /* Double-linked list of clauses waiting for a channel endpoint. */
    struct mill_clause *prev;
    struct mill_clause *next;
    /* The coroutine which created the clause. */
    struct mill_cr *cr;
    /* Channel endpoint the clause is waiting for. */
    struct mill_ep *ep;
    void *val;
    /* The case statement to jump to when the clause is chosen. */
    void *label;
    /* Linked list of clauses in the choose statement. */
    struct mill_clause *next_clause;
};

#define chmake(type, bufsz) mill_chmake(sizeof(type), bufsz)

#define chs(channel, type, value) \
    do {\
        type mill_val = (value);\
        mill_chs((channel), &mill_val, sizeof(type));\
    } while(0)

#define chr(channel, type) \
    (*(type*)mill_chr((channel), alloca(sizeof(type)), sizeof(type)))

chan mill_chmake(size_t sz, size_t bufsz);
chan chdup(chan ch);
void mill_chs(chan ch, void *val, size_t sz);
void *mill_chr(chan ch, void *val, size_t sz);
void chclose(chan ch);

#define mill_concat(x,y) x##y

#define choose \
    while(1) {\
        if(0)

#define mill_in(chan, type, name, idx) \
            break;\
        }\
        struct mill_clause mill_concat(mill_clause, idx);\
        type mill_concat(mill_val, idx);\
        mill_choose_in(\
            &mill_concat(mill_clause, idx),\
            (chan),\
            &mill_concat(mill_val, idx),\
            sizeof(type),\
            &&mill_concat(mill_label, idx));\
        if(0) {\
            type name;\
            mill_concat(mill_label, idx):\
            name = mill_concat(mill_val, idx);\
            mill_concat(mill_dummylabel, idx)

#define in(chan, type, name) mill_in((chan), type, name, __COUNTER__)

#define mill_out(chan, type, val, idx) \
            break;\
        }\
        struct mill_clause mill_concat(mill_clause, idx);\
        type mill_concat(mill_val, idx) = (val);\
        mill_choose_out(\
            &mill_concat(mill_clause, idx),\
            (chan),\
            &mill_concat(mill_val, idx),\
            sizeof(type),\
            &&mill_concat(mill_label, idx));\
        if(0) {\
            mill_concat(mill_label, idx):\
            mill_concat(mill_dummylabel, idx)

#define out(chan, type, val) mill_out((chan), type, (val), __COUNTER__)

#define mill_otherwise(idx) \
            break;\
        }\
        mill_choose_otherwise(&&mill_concat(mill_label, idx));\
        if(0) {\
            mill_concat(mill_label, idx)

#define otherwise mill_otherwise(__COUNTER__)

#define end \
            break;\
        }\
        goto *mill_choose_wait();

void mill_choose_in(struct mill_clause *clause,
    chan ch, void *val, size_t sz, void *label);
void mill_choose_out(struct mill_clause *clause,
    chan ch, void *val, size_t sz, void *label);
void mill_choose_otherwise(void *label);
void *mill_choose_wait(void);

/******************************************************************************/
/*  Library                                                                   */
/******************************************************************************/

void msleep(unsigned long ms);
chan after(unsigned long ms);

int msocket(int family, int type, int protocol);
int mconnect(int s, const struct sockaddr *addr, socklen_t addrlen);
int maccept(int s, struct sockaddr *addr, socklen_t *addrlen);
ssize_t msend(int s, const void *buf, size_t len, int flags);
ssize_t mrecv(int s, void *buf, size_t len, int flags);

#endif

