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

#include <errno.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

/******************************************************************************/
/*  ABI versioning support                                                    */
/******************************************************************************/

/*  Don't change this unless you know exactly what you're doing and have      */
/*  read and understand the following documents:                              */
/*  www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html     */
/*  www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html  */

/*  The current interface version. */
#define MILL_VERSION_CURRENT 15

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
/*  Helpers                                                                   */
/******************************************************************************/

MILL_EXPORT int64_t now(void);

/******************************************************************************/
/*  Coroutines                                                                */
/******************************************************************************/

MILL_EXPORT void goprepare(int count, size_t stack_size, size_t val_size);

MILL_EXPORT extern volatile int mill_unoptimisable1;
MILL_EXPORT extern volatile void *mill_unoptimisable2;

MILL_EXPORT sigjmp_buf *mill_getctx(void);
MILL_EXPORT __attribute__((noinline)) void *mill_go_prologue(
    const char *created);
MILL_EXPORT __attribute__((noinline)) void mill_go_epilogue(void);

#define mill_string2(x) #x
#define mill_string(x) mill_string2(x)

#if defined __GNUC__ || defined __clang__
#define coroutine __attribute__((noinline))
#else
#error "Unsupported compiler!"
#endif

#define go(fn) \
    do {\
        void *mill_sp;\
        if(!sigsetjmp(*mill_getctx(), 0)) {\
            mill_sp = mill_go_prologue(__FILE__ ":" mill_string(__LINE__));\
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

#define msleep(deadline) mill_msleep((deadline),\
    __FILE__ ":" mill_string(__LINE__))

MILL_EXPORT void mill_msleep(int64_t deadline, const char *current);

#define fdwait(fd, events, deadline) mill_fdwait((fd), (events), (deadline),\
    __FILE__ ":" mill_string(__LINE__))

MILL_EXPORT void fdclean(int fd);

#define FDW_IN 1
#define FDW_OUT 2
#define FDW_ERR 4

MILL_EXPORT int mill_fdwait(int fd, int events, int64_t deadline,
    const char *current);

MILL_EXPORT pid_t mfork(void);

MILL_EXPORT void *cls(void);
MILL_EXPORT void setcls(void *val);

/******************************************************************************/
/*  Channels                                                                  */
/******************************************************************************/

typedef struct mill_chan *chan;
typedef struct{void *f1; void *f2; void *f3; void *f4; \
    void *f5; void *f6; int f7; int f8; int f9;} mill_clause;
#define MILL_CLAUSELEN (sizeof(mill_clause))

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
                    name = *(type*)mill_choose_val(sizeof(type));\
                    goto mill_concat(mill_dummylabel, idx);\
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
                    goto mill_concat(mill_dummylabel, idx);\
                    mill_concat(mill_dummylabel, idx)

#define out(chan, type, val) mill_out((chan), type, (val), __COUNTER__)

#define mill_deadline(ddline, idx) \
                    break;\
                }\
                goto mill_concat(mill_label, idx);\
            }\
            mill_choose_deadline(ddline);\
            if(0) {\
                mill_concat(mill_label, idx):\
                if(mill_idx == -1) {\
                    goto mill_concat(mill_dummylabel, idx);\
                    mill_concat(mill_dummylabel, idx)

#define deadline(ddline) mill_deadline(ddline, __COUNTER__)

#define mill_otherwise(idx) \
                    break;\
                }\
                goto mill_concat(mill_label, idx);\
            }\
            mill_choose_otherwise();\
            if(0) {\
                mill_concat(mill_label, idx):\
                if(mill_idx == -1) {\
                    goto mill_concat(mill_dummylabel, idx);\
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
MILL_EXPORT void mill_choose_deadline(int64_t ddline);
MILL_EXPORT void mill_choose_otherwise(void);
MILL_EXPORT int mill_choose_wait(void);
MILL_EXPORT void *mill_choose_val(size_t sz);

/******************************************************************************/
/*  IP address library                                                        */
/******************************************************************************/

#define IPADDR_IPV4 1
#define IPADDR_IPV6 2
#define IPADDR_PREF_IPV4 3
#define IPADDR_PREF_IPV6 4
#define IPADDR_MAXSTRLEN 46

typedef struct {char data[32];} ipaddr;

MILL_EXPORT ipaddr iplocal(const char *name, int port, int mode);
MILL_EXPORT ipaddr ipremote(const char *name, int port, int mode,
    int64_t deadline);
MILL_EXPORT const char *ipaddrstr(ipaddr addr, char *ipstr);

/******************************************************************************/
/*  TCP library                                                               */
/******************************************************************************/

typedef struct mill_tcpsock *tcpsock;

MILL_EXPORT tcpsock tcplisten(ipaddr addr, int backlog);
MILL_EXPORT int tcpport(tcpsock s);
MILL_EXPORT tcpsock tcpaccept(tcpsock s, int64_t deadline);
MILL_EXPORT ipaddr tcpaddr(tcpsock s);
MILL_EXPORT tcpsock tcpconnect(ipaddr addr, int64_t deadline);
MILL_EXPORT size_t tcpsend(tcpsock s, const void *buf, size_t len,
    int64_t deadline);
MILL_EXPORT void tcpflush(tcpsock s, int64_t deadline);
MILL_EXPORT size_t tcprecv(tcpsock s, void *buf, size_t len, int64_t deadline);
MILL_EXPORT size_t tcprecvuntil(tcpsock s, void *buf, size_t len,
    const char *delims, size_t delimcount, int64_t deadline);
MILL_EXPORT void tcpclose(tcpsock s);

/******************************************************************************/
/*  UDP library                                                               */
/******************************************************************************/

typedef struct mill_udpsock *udpsock;

MILL_EXPORT udpsock udplisten(ipaddr addr);
MILL_EXPORT int udpport(udpsock s);
MILL_EXPORT void udpsend(udpsock s, ipaddr addr, const void *buf, size_t len);
MILL_EXPORT size_t udprecv(udpsock s, ipaddr *addr,
    void *buf, size_t len, int64_t deadline);
MILL_EXPORT void udpclose(udpsock s);

/******************************************************************************/
/*  UNIX library                                                              */
/******************************************************************************/

typedef struct mill_unixsock *unixsock;

MILL_EXPORT unixsock unixlisten(const char *addr, int backlog);
MILL_EXPORT unixsock unixaccept(unixsock s, int64_t deadline);
MILL_EXPORT unixsock unixconnect(const char *addr);
MILL_EXPORT void unixpair(unixsock *a, unixsock *b);
MILL_EXPORT size_t unixsend(unixsock s, const void *buf, size_t len,
    int64_t deadline);
MILL_EXPORT void unixflush(unixsock s, int64_t deadline);
MILL_EXPORT size_t unixrecv(unixsock s, void *buf, size_t len,
    int64_t deadline);
MILL_EXPORT size_t unixrecvuntil(unixsock s, void *buf, size_t len,
    const char *delims, size_t delimcount, int64_t deadline);
MILL_EXPORT void unixclose(unixsock s);

/******************************************************************************/
/*  File library                                                              */
/******************************************************************************/

typedef struct mill_file *mfile;

#define mfin mill_mfin()
#define mfout mill_mfout()
#define mferr mill_mferr()

MILL_EXPORT mfile mfopen(const char *pathname, int flags, mode_t mode);
MILL_EXPORT size_t mfwrite(mfile f, const void *buf, size_t len,
    int64_t deadline);
MILL_EXPORT void mfflush(mfile f, int64_t deadline);
MILL_EXPORT size_t mfread(mfile f, void *buf, size_t len, int64_t deadline);
MILL_EXPORT void mfclose(mfile f);
MILL_EXPORT off_t mftell(mfile f);
MILL_EXPORT off_t mfseek(mfile f, off_t offset);
MILL_EXPORT int mfeof(mfile f);
MILL_EXPORT mfile mill_mfin(void);
MILL_EXPORT mfile mill_mfout(void);
MILL_EXPORT mfile mill_mferr(void);

/******************************************************************************/
/*  Debugging                                                                 */
/******************************************************************************/

MILL_EXPORT void goredump(void);
MILL_EXPORT void gotrace(int level);

#endif
