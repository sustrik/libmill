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

#include "model.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>

void goredump(void) {
    char buf[256];
    fprintf(stderr,
        "\nCOROUTINE   state                  current              created\n");
    fprintf(stderr,
        "---------------------------------------------------------------\n");
    struct mill_list_item *it;
    for(it = mill_list_begin(&all_crs); it != NULL; it = mill_list_next(it)) {
        struct mill_cr *cr = mill_cont(it, struct mill_cr, all_crs_item);
        switch(cr->state) {
        case MILL_YIELD:
            sprintf(buf, "%s", first_cr == cr ? "running" : "yield()");
            break;
        case MILL_MSLEEP:
            sprintf(buf, "msleep()");
            break;
        case MILL_FDWAIT:
            sprintf(buf, "fdwait(%d)", -1);
            break;
        case MILL_CHR:
            sprintf(buf, "chr(%d)", mill_getchan(mill_cont(mill_slist_begin(
                &cr->chstate.clauses), struct mill_clause, chitem)->ep)->id);
            break;
        case MILL_CHS:
            sprintf(buf, "chs(%d)", mill_getchan(mill_cont(mill_slist_begin(
                &cr->chstate.clauses), struct mill_clause, chitem)->ep)->id);
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
		            pos += sprintf(&buf[pos], "%d", mill_getchan(mill_cont(it,
                        struct mill_clause, chitem)->ep)->id);
		        }
		        sprintf(&buf[pos], ")");
            }
            break;
        default:
            assert(0);
        }
        fprintf(stderr, "%s (%06d) %-22s %-20s %s\n",
            cr == first_cr ? "=>" : "  ",
            cr->id,
            buf,
            cr == first_cr ? "---" : cr->current,
            cr->created ? cr->created : "<main>");
    }
    fprintf(stderr,"\n");
    if(mill_list_empty(&all_chans))
        return;
    fprintf(stderr,
        "CHANNEL  msgs/max    senders/receivers      refs  done  created\n");
    fprintf(stderr,
        "---------------------------------------------------------------\n");
    for(it = mill_list_begin(&all_chans); it != NULL; it = mill_list_next(it)) {
        struct mill_chan *ch = mill_cont(it, struct mill_chan, all_chans_item);
        sprintf(buf, "%d/%d",
            (int)ch->items,
            (int)ch->bufsz);
        fprintf(stderr, "(%06d) %-11s ",
            ch->id,
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
            pos += sprintf(&buf[pos], "%d", (int)cl->cr->id);
            cl = mill_cont(mill_list_next(&cl->epitem),
                struct mill_clause, epitem);
        }
        fprintf(stderr, "%-22s %-5d %-5s %s\n",
            buf,
            (int)ch->refcount,
            ch->done ? "yes" : "no",
            ch->created);            
    }
    fprintf(stderr,"\n");
}

void mill_preserve_debug(void) {
    /* Do nothing, but trick the copiler into thinking that
       the debug functions are being used. */
    static volatile int unoptimisable = 0;
    if(unoptimisable) {
        goredump();
    }
}

