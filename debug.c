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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "chan.h"
#include "cr.h"
#include "libmill.h"
#include "list.h"
#include "stack.h"
#include "utils.h"

/* ID to be assigned to next launched coroutine. */
static int mill_next_cr_id = 1;

/* List of all coroutines. */
static struct mill_list mill_all_crs = {
    &mill_main.debug.item, &mill_main.debug.item};

/* ID to be assigned to the next created channel. */
static int mill_next_chan_id = 1;

/* List of all channels. */
static struct mill_list mill_all_chans = {0};

void mill_panic(const char *text) {
    fprintf(stderr, "panic: %s\n", text);
    abort();
}

void mill_register_cr(struct mill_debug_cr *cr, const char *created) {
    mill_list_insert(&mill_all_crs, &cr->item, NULL);
    cr->id = mill_next_cr_id;
    ++mill_next_cr_id;
    cr->created = created;
    cr->current = NULL;
}

void mill_unregister_cr(struct mill_debug_cr *cr) {
    mill_list_erase(&mill_all_crs, &cr->item);
}

void mill_register_chan(struct mill_debug_chan *ch, const char *created) {
    mill_list_insert(&mill_all_chans, &ch->item, NULL);
    ch->id = mill_next_chan_id;
    ++mill_next_chan_id;
    ch->created = created;
}

void mill_unregister_chan(struct mill_debug_chan *ch) {
    mill_list_erase(&mill_all_chans, &ch->item);
}

void mill_set_current(struct mill_debug_cr *cr, const char *current) {
    cr->current = current;
}

void goredump(void) {
    char buf[256];
    char idbuf[10];

    fprintf(stderr,
        "\nCOROUTINE  state                                      "
        "current                                  created\n");
    fprintf(stderr,
        "----------------------------------------------------------------------"
        "--------------------------------------------------\n");
    struct mill_list_item *it;
    for(it = mill_list_begin(&mill_all_crs); it; it = mill_list_next(it)) {
        struct mill_cr *cr = mill_cont(it, struct mill_cr, debug.item);
        switch(cr->state) {
        case MILL_READY:
            sprintf(buf, "%s", mill_running == cr ? "RUNNING" : "ready");
            break;
        case MILL_MSLEEP:
            sprintf(buf, "msleep()");
            break;
        case MILL_FDWAIT:
            sprintf(buf, "fdwait(%d, %s)", cr->fd,
                (cr->events & FDW_IN) &&
                    (cr->events & FDW_OUT) ? "FDW_IN | FDW_OUT" :
                cr->events & FDW_IN ? "FDW_IN" :
                cr->events & FDW_OUT ? "FDW_OUT" : 0);
            break;
        case MILL_CHR:
        case MILL_CHS:
        case MILL_CHOOSE:
            {
                int pos = 0;
                if(cr->state == MILL_CHR)
                    pos += sprintf(&buf[pos], "chr(");
                else if(cr->state == MILL_CHS)
                    pos += sprintf(&buf[pos], "chs(");
                else
                    pos += sprintf(&buf[pos], "choose(");
                int first = 1;
                struct mill_slist_item *it;
                for(it = mill_slist_begin(&cr->choosedata.clauses); it;
                      it = mill_slist_next(it)) {
                if(first)
                    first = 0;
                else
                    pos += sprintf(&buf[pos], ",");
                pos += sprintf(&buf[pos], "<%d>", mill_getchan(
                        mill_cont(it, struct mill_clause,
                        chitem)->ep)->debug.id);
            }
            sprintf(&buf[pos], ")");
            }
            break;
        default:
            assert(0);
        }
        snprintf(idbuf, sizeof(idbuf), "{%d}", (int)cr->debug.id);
        fprintf(stderr, "%-8s   %-42s %-40s %s\n",
            idbuf,
            buf,
            cr == mill_running ? "---" : cr->debug.current,
            cr->debug.created ? cr->debug.created : "<main>");
    }
    fprintf(stderr,"\n");

    if(mill_list_empty(&mill_all_chans))
        return;
    fprintf(stderr,
        "CHANNEL  msgs/max    senders/receivers                          "
        "refs  done  created\n");
    fprintf(stderr,
        "----------------------------------------------------------------------"
        "--------------------------------------------------\n");
    for(it = mill_list_begin(&mill_all_chans); it; it = mill_list_next(it)) {
        struct mill_chan_ *ch = mill_cont(it, struct mill_chan_, debug.item);
        snprintf(idbuf, sizeof(idbuf), "<%d>", (int)ch->debug.id);
        sprintf(buf, "%d/%d",
            (int)ch->items,
            (int)ch->bufsz);
        fprintf(stderr, "%-8s %-11s ",
            idbuf,
            buf);
        int pos;
        struct mill_list *clauselist;
        if(!mill_list_empty(&ch->sender.clauses)) {
            pos = sprintf(buf, "s:");
            clauselist = &ch->sender.clauses;
        }
        else if(!mill_list_empty(&ch->receiver.clauses)) {
            pos = sprintf(buf, "r:");
            clauselist = &ch->receiver.clauses;
        }
        else {
            sprintf(buf, " ");
            clauselist = NULL;
        }
        struct mill_clause *cl = NULL;
        if(clauselist)
            cl = mill_cont(mill_list_begin(clauselist),
                struct mill_clause, epitem);
        int first = 1;
        while(cl) {
            if(first)
                first = 0;
            else
                pos += sprintf(&buf[pos], ",");
            pos += sprintf(&buf[pos], "{%d}", (int)cl->cr->debug.id);
            cl = mill_cont(mill_list_next(&cl->epitem),
                struct mill_clause, epitem);
        }
        fprintf(stderr, "%-42s %-5d %-5s %s\n",
            buf,
            (int)ch->refcount,
            ch->done ? "yes" : "no",
            ch->debug.created);
    }
    fprintf(stderr,"\n");
}

int mill_tracelevel = 0;

void gotrace(int level) {
    mill_tracelevel = level;
}

void mill_trace_(const char *location, const char *format, ...) {
    if(mill_fast(mill_tracelevel <= 0))
        return;

    char buf[256];

    /* First print the timestamp. */
    struct timeval nw;
    gettimeofday(&nw, NULL);
    struct tm *nwtm = localtime(&nw.tv_sec);
    snprintf(buf, sizeof buf, "%02d:%02d:%02d",
        (int)nwtm->tm_hour, (int)nwtm->tm_min, (int)nwtm->tm_sec);
    fprintf(stderr, "==> %s.%06d ", buf, (int)nw.tv_usec);

    /* Coroutine ID. */
    snprintf(buf, sizeof(buf), "{%d}", (int)mill_running->debug.id);
    fprintf(stderr, "%-8s ", buf);

    va_list va;
    va_start(va ,format);
    vfprintf(stderr, format, va);
    va_end(va);
    if(location)
        fprintf(stderr, " at %s\n", location);
    else
        fprintf(stderr, "\n");
    fflush(stderr);
}

void mill_preserve_debug(void) {
    /* Do nothing, but trick the compiler into thinking that the debug
       functions are being used so that it does not optimise them away. */
    static volatile int unoptimisable = 1;
    if(unoptimisable)
        return;
    goredump();
    gotrace(0);
}

int mill_hascrs(void) {
    return (mill_all_crs.first == &mill_main.debug.item &&
        mill_all_crs.last == &mill_main.debug.item) ? 0 : 1;
}

