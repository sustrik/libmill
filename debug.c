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

#include "libmill.h"
#include "model.h"
#include "utils.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

/* ID to be assigned to next launched coroutine. */
static int mill_next_cr_id = 1;

/* List of all coroutines. */
static struct mill_list mill_all_crs = {
    &main_cr.debug.item, &main_cr.debug.item};

/* ID to be assigned to the next created channel. */
static int mill_next_chan_id = 1;

/* List of all channels. */
static struct mill_list mill_all_chans = {0};

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
        case MILL_YIELD:
            sprintf(buf, "%s", first_cr == cr ? "RUNNING" : "yield()");
            break;
        case MILL_MSLEEP:
            sprintf(buf, "msleep()");
            break;
        case MILL_FDWAIT:
            sprintf(buf, "fdwait(%d)", -1);
            break;
        case MILL_CHR:
            sprintf(buf, "chr(<%d>)", mill_getchan(mill_cont(mill_slist_begin(
                &cr->chstate.clauses), struct mill_clause,
                chitem)->ep)->debug.id);
            break;
        case MILL_CHS:
            sprintf(buf, "chs(<%d>)", mill_getchan(mill_cont(mill_slist_begin(
                &cr->chstate.clauses), struct mill_clause,
                chitem)->ep)->debug.id);
            break;
        case MILL_CHOOSE:
            {
		        int pos = 0;
                pos += sprintf(&buf[pos], "choose(");
		        int first = 1;
                struct mill_slist_item *it;
                for(it = mill_slist_begin(&cr->chstate.clauses); it;
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
            cr == first_cr ? "---" : cr->debug.current,
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
        struct mill_chan *ch = mill_cont(it, struct mill_chan, debug.item);
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

void mill_preserve_debug(void) {
    /* Do nothing, but trick the copiler into thinking that
       the debug functions are being used. */
    static volatile int unoptimisable = 1;
    if(unoptimisable)
        return;
    goredump();
    trace(0);
}

static int tracelevel = 0;
static struct mill_cr *mill_last_traced_cr = NULL;

void trace(int level) {
    tracelevel = level;
}

void mill_trace(const char *location, const char *format, ...) {
    if(tracelevel <= 0)
        return;

    char buf[256];

    if(mill_last_traced_cr && mill_last_traced_cr != first_cr)
        fprintf(stderr, "==> --------------------------------------------------"
        "------------------------------------------------------------------\n");
    mill_last_traced_cr = first_cr;
    
    /* First print the timestamp. */
    struct timeval nw;
    gettimeofday(&nw, NULL);
    struct tm *nwtm = localtime(&nw.tv_sec);
    strftime(buf, sizeof buf, "%02H:%02M:%02S", nwtm);
    fprintf(stderr, "==> %s.%06d ", buf, (int)nw.tv_usec);

    /* Coroutine ID. */
    snprintf(buf, sizeof(buf), "{%d}", (int)first_cr->debug.id);
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

