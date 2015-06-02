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

int next_cr_id = 1;
struct mill_cr main_cr = {NULL};
struct mill_list all_crs = {&main_cr.all_crs_item, &main_cr.all_crs_item};
struct mill_cr *first_cr = &main_cr;
struct mill_cr *last_cr = &main_cr;
struct mill_slist sleeping = {0};
int wait_size = 0;
int wait_capacity = 0;
struct pollfd *wait_fds = NULL;
struct mill_fdwitem *wait_items = NULL;
int mill_next_chan_id = 1;
struct mill_list all_chans = {0};

struct mill_chan *mill_getchan(struct mill_ep *ep) {
    switch(ep->type) {
    case MILL_SENDER:
        return mill_cont(ep, struct mill_chan, sender);
    case MILL_RECEIVER:
        return mill_cont(ep, struct mill_chan, receiver);
    default:
        assert(0);
    }
}

