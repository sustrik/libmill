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

#ifndef LIBMILL_H_INCLUDED
#define LIBMILL_H_INCLUDED

#include <alloca.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>

/******************************************************************************/
/*  ABI versioning support                                                    */
/******************************************************************************/

/*  Don't change this unless you know exactly what you're doing and have      */
/*  read and understand the following documents:                              */
/*  www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html     */
/*  www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html  */

/*  The current interface version. */
#define MILL_VERSION_CURRENT 4

/*  The latest revision of the current interface. */
#define MILL_VERSION_REVISION 3

/*  How many past interface versions are still supported. */
#define MILL_VERSION_AGE 0

/******************************************************************************/
/*  Symbol visibility                                                         */
/******************************************************************************/

#if defined MILL_NO_EXPORTS
#   define MILL_EXPORT
#else
#   if defined _WIN32
#      if defined MILL_EXPORTS
#          define MILL_EXPORT __declspec(dllexport)
#      else
#          define MILL_EXPORT __declspec(dllimport)
#      endif
#   else
#      if defined __SUNPRO_C
#          define MILL_EXPORT __global
#      elif (defined __GNUC__ && __GNUC__ >= 4) || \
             defined __INTEL_COMPILER || defined __clang__
#          define MILL_EXPORT __attribute__ ((visibility("default")))
#      else
#          define MILL_EXPORT
#      endif
#   endif
#endif

/******************************************************************************/
/*  Coroutines                                                                */
/******************************************************************************/

MILL_EXPORT extern volatile int mill_unoptimisable1;
MILL_EXPORT extern volatile void *mill_unoptimisable2;

MILL_EXPORT void *mill_go_prologue(const char *created);
MILL_EXPORT void mill_go_epilogue(void);

#define mill_string2(x) #x
#define mill_string(x) mill_string2(x)

#define go(fn) \
    do {\
        void *mill_sp = mill_go_prologue(__FILE__ ":" mill_string(__LINE__));\
        if(mill_sp) {\
            int mill_anchor[mill_unoptimisable1];\
            mill_unoptimisable2 = &mill_anchor;\
            char mill_filler[(char*)&mill_anchor - (char*)(mill_sp)];\
            mill_unoptimisable2 = &mill_filler;\
            fn;\
            mill_go_epilogue();\
        }\
    } while(0)

#define yield() mill_yield(__FILE__ ":" mill_string(__LINE__))

MILL_EXPORT void mill_yield(const char *current);

#define msleep(ms) mill_msleep((ms), __FILE__ ":" mill_string(__LINE__))

MILL_EXPORT void mill_msleep(long ms, const char *current);

#define fdwait(fd, events, timeout) mill_fdwait((fd), (events), (timeout),\
    __FILE__ ":" mill_string(__LINE__))

#define FDW_IN 1
#define FDW_OUT 2
#define FDW_ERR 4

MILL_EXPORT int mill_fdwait(int fd, int events, long timeout,
    const char *current);

MILL_EXPORT void *cls(void);
MILL_EXPORT void setcls(void *val);

/******************************************************************************/
/*  Channels                                                                  */
/******************************************************************************/

typedef struct mill_chan *chan;

#define MILL_CLAUSELEN (sizeof(struct{void *f1; void *f2; void *f3; void *f4; \
    void *f5; void *f6; int f7; int f8; int f9;}))

#define chmake(type, bufsz) mill_chmake(sizeof(type), bufsz,\
    __FILE__ ":" mill_string(__LINE__))

#define chdup(channel) mill_chdup((channel),\
    __FILE__ ":" mill_string(__LINE__))

#define chs(channel, type, value) \
    do {\
        type mill_val = (value);\
        mill_chs((channel), &mill_val, sizeof(type),\
            __FILE__ ":" mill_string(__LINE__));\
    } while(0)

#define chr(channel, type) \
    (*(type*)mill_chr((channel), sizeof(type),\
        __FILE__ ":" mill_string(__LINE__)))

#define chdone(channel, type, value) \
    do {\
        type mill_val = (value);\
        mill_chdone((channel), &mill_val, sizeof(type),\
             __FILE__ ":" mill_string(__LINE__));\
    } while(0)

#define chclose(channel) mill_chclose((channel),\
    __FILE__ ":" mill_string(__LINE__))

MILL_EXPORT chan mill_chmake(size_t sz, size_t bufsz, const char *created);
MILL_EXPORT chan mill_chdup(chan ch, const char *created);
MILL_EXPORT void mill_chs(chan ch, void *val, size_t sz, const char *current);
MILL_EXPORT void *mill_chr(chan ch, size_t sz, const char *current);
MILL_EXPORT void mill_chdone(chan ch, void *val, size_t sz,
    const char *current);
MILL_EXPORT void mill_chclose(chan ch, const char *current);

#define mill_concat(x,y) x##y

#define choose \
    {\
        mill_choose_init(__FILE__ ":" mill_string(__LINE__));\
        int mill_idx = -2;\
        while(1) {\
            if(mill_idx != -2) {\
                if(0)

#define mill_in(chan, type, name, idx) \
                    break;\
                }\
                goto mill_concat(mill_label, idx);\
            }\
            char mill_concat(mill_clause, idx)[MILL_CLAUSELEN];\
            mill_choose_in(\
                &mill_concat(mill_clause, idx)[0],\
                (chan),\
                sizeof(type),\
                idx);\
            if(0) {\
                type name;\
                mill_concat(mill_label, idx):\
                if(mill_idx == idx) {\
                    name = *(type*)mill_choose_val();\
                    mill_concat(mill_dummylabel, idx)

#define in(chan, type, name) mill_in((chan), type, name, __COUNTER__)

#define mill_out(chan, type, val, idx) \
                    break;\
                }\
                goto mill_concat(mill_label, idx);\
            }\
            char mill_concat(mill_clause, idx)[MILL_CLAUSELEN];\
            type mill_concat(mill_val, idx) = (val);\
            mill_choose_out(\
                &mill_concat(mill_clause, idx)[0],\
                (chan),\
                &mill_concat(mill_val, idx),\
                sizeof(type),\
                idx);\
            if(0) {\
                mill_concat(mill_label, idx):\
                if(mill_idx == idx) {\
                    mill_concat(mill_dummylabel, idx)

#define out(chan, type, val) mill_out((chan), type, (val), __COUNTER__)

#define mill_otherwise(idx) \
                    break;\
                }\
                goto mill_concat(mill_label, idx);\
            }\
            mill_choose_otherwise();\
            if(0) {\
                mill_concat(mill_label, idx):\
                if(mill_idx == -1) {\
                    mill_concat(mill_dummylabel, idx)

#define otherwise mill_otherwise(__COUNTER__)

#define end \
                    break;\
                }\
            }\
            mill_idx = mill_choose_wait();\
        }

MILL_EXPORT void mill_choose_init(const char *current);
MILL_EXPORT void mill_choose_in(void *clause, chan ch, size_t sz, int idx);
MILL_EXPORT void mill_choose_out(void *clause, chan ch, void *val, size_t sz,
    int idx);
MILL_EXPORT void mill_choose_otherwise(void);
MILL_EXPORT int mill_choose_wait(void);
MILL_EXPORT void *mill_choose_val(void);

/******************************************************************************/
/*  TCP library                                                               */
/******************************************************************************/

typedef struct tcpsock *tcpsock;

MILL_EXPORT tcpsock tcplisten(const char *addr);
MILL_EXPORT tcpsock tcpaccept(tcpsock s);
MILL_EXPORT tcpsock tcpconnect(const char *addr);
MILL_EXPORT void tcpsend(tcpsock s, const void *buf, size_t len);
MILL_EXPORT int tcpflush(tcpsock s);
MILL_EXPORT ssize_t tcprecv(tcpsock s, void *buf, size_t len);
MILL_EXPORT ssize_t tcprecvuntil(tcpsock s, void *buf, size_t len, char until);
MILL_EXPORT void tcpclose(tcpsock s);

/******************************************************************************/
/*  Debugging                                                                 */
/******************************************************************************/

MILL_EXPORT void goredump(void);
MILL_EXPORT void trace(int level);

#endif

