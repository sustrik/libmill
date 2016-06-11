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

#if defined(__cplusplus)
extern "C" {
#endif

/******************************************************************************/
/*  ABI versioning support                                                    */
/******************************************************************************/

/*  Don't change this unless you know exactly what you're doing and have      */
/*  read and understand the following documents:                              */
/*  www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html     */
/*  www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html  */

/*  The current interface version. */
#define MILL_VERSION_CURRENT 16

/*  The latest revision of the current interface. */
#define MILL_VERSION_REVISION 2

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

MILL_EXPORT int64_t mill_now(void);

/******************************************************************************/
/*  Coroutines                                                                */
/******************************************************************************/

MILL_EXPORT void mill_goprepare(int count, size_t stack_size, size_t val_size);

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

#define MILL_HERE (__FILE__ ":" mill_string(__LINE__))

#define mill_go(fn) \
    do {\
        void *mill_sp;\
        if(!sigsetjmp(*mill_getctx(), 0)) {\
            mill_sp = mill_go_prologue(MILL_HERE);\
            int mill_anchor[mill_unoptimisable1];\
            mill_unoptimisable2 = &mill_anchor;\
            char mill_filler[(char*)&mill_anchor - (char*)(mill_sp)];\
            mill_unoptimisable2 = &mill_filler;\
            fn;\
            mill_go_epilogue();\
        }\
    } while(0)

#define yield() mill_yield(MILL_HERE)

MILL_EXPORT void mill_yield(const char *current);

#define msleep(deadline) mill_msleep((deadline), MILL_HERE)

MILL_EXPORT void mill_msleep(int64_t deadline, const char *current);

#define fdwait(fd, events, deadline) \
    mill_fdwait((fd), (events), (deadline), MILL_HERE)

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

#define chmake(type, bufsz) mill_chmake(sizeof(type), bufsz, MILL_HERE)

#define chdup(channel) mill_chdup((channel), MILL_HERE)

#define chs(channel, type, value) \
    do {\
        type mill_val = (value);\
        mill_chs((channel), &mill_val, sizeof(type), MILL_HERE);\
    } while(0)

#define chr(channel, type) \
    (*(type*)mill_chr((channel), sizeof(type), MILL_HERE))

#define chdone(channel, type, value) \
    do {\
        type mill_val = (value);\
        mill_chdone((channel), &mill_val, sizeof(type), MILL_HERE);\
    } while(0)

#define chclose(channel) mill_chclose((channel), MILL_HERE)

MILL_EXPORT chan mill_chmake(size_t sz, size_t bufsz, const char *created);
MILL_EXPORT chan mill_chdup(chan ch, const char *created);
MILL_EXPORT void mill_chs(chan ch, void *val, size_t sz, const char *current);
MILL_EXPORT void *mill_chr(chan ch, size_t sz, const char *current);
MILL_EXPORT void mill_chdone(chan ch, void *val, size_t sz,
    const char *current);
MILL_EXPORT void mill_chclose(chan ch, const char *current);

#define mill_concat(x,y) x##y

#define mill_choose \
    {\
        mill_choose_init(MILL_HERE);\
        int mill_idx = -2;\
        while(1) {\
            if(mill_idx != -2) {\
                if(0)

#define mill_internal__in(chan, type, name, idx) \
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

#define mill_internal__out(chan, type, val, idx) \
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

#define mill_internal__deadline(ddline, idx) \
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

#define mill_internal__otherwise(idx) \
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

#define mill_end \
                    break;\
                }\
            }\
            mill_idx = mill_choose_wait();\
        }

#if defined MILL_USE_PREFIX
# define mill_in(chan, type, name) mill_internal__in((chan), type, name, __COUNTER__)
# define mill_out(chan, type, val) mill_internal__out((chan), type, (val), __COUNTER__)
# define mill_deadline(ddline) mill_internal__deadline(ddline, __COUNTER__)
# define mill_otherwise mill_internal__otherwise(__COUNTER__)
#else
# define now mill_now
# define goprepare(cnt, stcksz, valsz) mill_goprepare((cnt), (stcksz), (valsz))
# define go(fn) mill_go(fn)
# define choose mill_choose
# define end mill_end
# define in(chan, type, name) mill_internal__in((chan), type, name, __COUNTER__)
# define out(chan, type, val) mill_internal__out((chan), type, (val), __COUNTER__)
# define deadline(ddline) mill_internal__deadline(ddline, __COUNTER__)
# define otherwise mill_internal__otherwise(__COUNTER__)
#endif

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

typedef struct mill_tcpsock *mill_tcpsock_;

MILL_EXPORT mill_tcpsock_ mill_tcplisten_(ipaddr addr, int backlog);
MILL_EXPORT int mill_tcpport_(mill_tcpsock_ s);
MILL_EXPORT mill_tcpsock_ mill_tcpaccept_(mill_tcpsock_ s, int64_t deadline);
MILL_EXPORT ipaddr mill_tcpaddr_(mill_tcpsock_ s);
MILL_EXPORT mill_tcpsock_ mill_tcpconnect_(ipaddr addr, int64_t deadline);
MILL_EXPORT size_t mill_tcpsend_(mill_tcpsock_ s, const void *buf, size_t len,
    int64_t deadline);
MILL_EXPORT void mill_tcpflush_(mill_tcpsock_ s, int64_t deadline);
MILL_EXPORT size_t mill_tcprecv_(mill_tcpsock_ s, void *buf, size_t len,
    int64_t deadline);
MILL_EXPORT size_t mill_tcprecvuntil_(mill_tcpsock_ s, void *buf, size_t len,
    const char *delims, size_t delimcount, int64_t deadline);
MILL_EXPORT void mill_tcpclose_(mill_tcpsock_ s);

#if defined MILL_USE_PREFIX
#define mill_tcpsock mill_tcpsock_
#define mill_tcplisten mill_tcplisten_
#define mill_tcpport mill_tcpport_
#define mill_tcpaccept mill_tcpaccept_
#define mill_tcpaddr mill_tcpaddr_
#define mill_tcpconnect mill_tcpconnect_
#define mill_tcpsend mill_tcpsend_
#define mill_tcpflush mill_tcpflush_
#define mill_tcprecv mill_tcprecv_
#define mill_tcprecvuntil mill_tcprecvuntil_
#define mill_tcpclose mill_tcpclose_
#else
#define tcpsock mill_tcpsock_
#define tcplisten mill_tcplisten_
#define tcpport mill_tcpport_
#define tcpaccept mill_tcpaccept_
#define tcpaddr mill_tcpaddr_
#define tcpconnect mill_tcpconnect_
#define tcpsend mill_tcpsend_
#define tcpflush mill_tcpflush_
#define tcprecv mill_tcprecv_
#define tcprecvuntil mill_tcprecvuntil_
#define tcpclose mill_tcpclose_
#endif

/******************************************************************************/
/*  UDP library                                                               */
/******************************************************************************/

typedef struct mill_udpsock *mill_udpsock_;

MILL_EXPORT mill_udpsock_ mill_udplisten_(ipaddr addr);
MILL_EXPORT int mill_udpport_(mill_udpsock_ s);
MILL_EXPORT void mill_udpsend_(mill_udpsock_ s, ipaddr addr,
    const void *buf, size_t len);
MILL_EXPORT size_t mill_udprecv_(mill_udpsock_ s, ipaddr *addr,
    void *buf, size_t len, int64_t deadline);
MILL_EXPORT void mill_udpclose_(mill_udpsock_ s);

#if defined MILL_USE_PREFIX
#define mill_udpsock mill_udpsock_
#define mill_udplisten mill_udplisten_
#define mill_udpport mill_udpport_
#define mill_udpsend mill_udpsend_
#define mill_udprecv mill_udprecv_
#define mill_udpclose mill_udpclose_
#else
#define udpsock mill_udpsock_
#define udplisten mill_udplisten_
#define udpport mill_udpport_
#define udpsend mill_udpsend_
#define udprecv mill_udprecv_
#define udpclose mill_udpclose_
#endif

/******************************************************************************/
/*  UNIX library                                                              */
/******************************************************************************/

typedef struct mill_unixsock *mill_unixsock_;

MILL_EXPORT mill_unixsock_ mill_unixlisten_(const char *addr, int backlog);
MILL_EXPORT mill_unixsock_ mill_unixaccept_(mill_unixsock_ s, int64_t deadline);
MILL_EXPORT mill_unixsock_ mill_unixconnect_(const char *addr);
MILL_EXPORT void mill_unixpair_(mill_unixsock_ *a, mill_unixsock_ *b);
MILL_EXPORT size_t mill_unixsend_(mill_unixsock_ s, const void *buf, size_t len,
    int64_t deadline);
MILL_EXPORT void mill_unixflush_(mill_unixsock_ s, int64_t deadline);
MILL_EXPORT size_t mill_unixrecv_(mill_unixsock_ s, void *buf, size_t len,
    int64_t deadline);
MILL_EXPORT size_t mill_unixrecvuntil_(mill_unixsock_ s, void *buf, size_t len,
    const char *delims, size_t delimcount, int64_t deadline);
MILL_EXPORT void mill_unixclose_(mill_unixsock_ s);

#if defined MILL_USE_PREFIX
#define mill_unixsock mill_unixsock_
#define mill_unixlisten mill_unixlisten_
#define mill_unixaccept mill_unixaccept_
#define mill_unixconnect mill_unixconnect_
#define mill_unixpair mill_unixpair_
#define mill_unixsend mill_unixsend_
#define mill_unixflush mill_unixflush_
#define mill_unixrecv mill_unixrecv_
#define mill_unixrecvuntil mill_unixrecvuntil_
#define mill_unixclose mill_unixclose_
#else
#define unixsock mill_unixsock_
#define unixlisten mill_unixlisten_
#define unixaccept mill_unixaccept_
#define unixconnect mill_unixconnect_
#define unixpair mill_unixpair_
#define unixsend mill_unixsend_
#define unixflush mill_unixflush_
#define unixrecv mill_unixrecv_
#define unixrecvuntil mill_unixrecvuntil_
#define unixclose mill_unixclose_
#endif

/******************************************************************************/
/*  File library                                                              */
/******************************************************************************/

typedef struct mill_file *mill_mfile_;

MILL_EXPORT mill_mfile_ mill_mfopen_(const char *pathname, int flags,
    mode_t mode);
MILL_EXPORT size_t mill_mfwrite_(mill_mfile_ f, const void *buf, size_t len,
    int64_t deadline);
MILL_EXPORT void mill_mfflush_(mill_mfile_ f, int64_t deadline);
MILL_EXPORT size_t mill_mfread_(mill_mfile_ f, void *buf, size_t len,
    int64_t deadline);
MILL_EXPORT void mill_mfclose_(mill_mfile_ f);
MILL_EXPORT off_t mill_mftell_(mill_mfile_ f);
MILL_EXPORT off_t mill_mfseek_(mill_mfile_ f, off_t offset);
MILL_EXPORT int mill_mfeof_(mill_mfile_ f);
MILL_EXPORT mill_mfile_ mill_mfin_(void);
MILL_EXPORT mill_mfile_ mill_mfout_(void);
MILL_EXPORT mill_mfile_ mill_mferr_(void);

#if defined MILL_USE_PREFIX
#define mill_mfile mill_mfile_
#define mill_mfopen mill_mfopen_
#define mill_mfwrite mill_mfwrite_
#define mill_mfflush mill_mfflush_
#define mill_mfread mill_mfread_
#define mill_mfclose mill_mfclose_
#define mill_mftell mill_mftell_
#define mill_mfseek mill_mfseek_
#define mill_mfeof mill_mfeof_
#define mill_mfin mill_mfin_()
#define mill_mfout mill_mfout_()
#define mill_mferr mill_mferr_()
#else
#define mfile mill_mfile_
#define mfopen mill_mfopen_
#define mfwrite mill_mfwrite_
#define mfflush mill_mfflush_
#define mfread mill_mfread_
#define mfclose mill_mfclose_
#define mftell mill_mftell_
#define mfseek mill_mfseek_
#define mfeof mill_mfeof_
#define mfin mill_mfin_()
#define mfout mill_mfout_()
#define mferr mill_mferr_()
#endif

/******************************************************************************/
/*  Debugging                                                                 */
/******************************************************************************/

/* These symbols are not wrapped in macros so that they can be used
   directly from the debugger. */
MILL_EXPORT void goredump(void);
MILL_EXPORT void gotrace(int level);

#if defined MILL_USE_PREFIX
#define mill_goredump goredump
#define mill_gotrace gotrace
#endif

#if defined(__cplusplus)
}
#endif

#endif
