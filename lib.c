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

void musleep(unsigned int microseconds) {
    if (!ring)
        init();
    if(microseconds <= 0)
        return;
    uint64_t ms = microseconds / 1000;
    if(microseconds % 1000 != 0)
        ++ms;
    dotrace("goes to sleep");
    struct cr *current = ring_rm();
    current->expiry = now() + ms;

    struct cr **it = &timers;
    while(*it && (*it)->expiry <= current->expiry)
        it = &((*it)->next);
    current->next = *it;
    *it = current;
    if(setjmp(current->ctx))
        return;
    schedule();    
}

void msleep(unsigned int seconds) {
    musleep(seconds * 1000000);
}

static void after_(int ms, chan ch) {
    musleep(ms * 1000);
    chan_val val;
    val.ptr = NULL;
    chan_send(ch, val);
    chan_close(ch);
}

chan after(int ms) {
    chan ch = chan_init();
    go(after_(ms, ch));
    return ch;
}

