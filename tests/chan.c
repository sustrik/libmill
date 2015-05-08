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
#include <stdio.h>
#include "../mill.h"

struct foo {
    int first;
    int second;
};

void sender(chan ch, int doyield, int val) {
    if(doyield)
        yield();
    chs(ch, int, val);
    chclose(ch); 
}

void receiver(chan ch, int expected) {
    int val = chr(ch, int);
    assert(val == expected);
    chclose(ch);
}

void charsender(chan ch, char val) {
    chs(ch, char, val);
    chclose(ch);
}

void structsender(chan ch, struct foo val) {
    chs(ch, struct foo, val);
    chclose(ch);
}

int main() {
    int val;

    /* Receiver waits for sender. */
    chan ch1 = chmake(int, 0);
    go(sender(chdup(ch1), 1, 333));
    val = chr(ch1, int);
    assert(val == 333);
    chclose(ch1);

    /* Sender waits for receiver. */
    chan ch2 = chmake(int, 0);
    go(sender(chdup(ch2), 0, 444));
    val = chr(ch2, int);
    assert(val == 444);
    chclose(ch2);

    /* Test two simultaneous senders. */
    chan ch3 = chmake(int, 0);
    go(sender(chdup(ch3), 0, 888));
    go(sender(chdup(ch3), 0, 999));
    val = chr(ch3, int);
    assert(val == 888);
    val = chr(ch3, int);
    assert(val == 999);
    chclose(ch3);

    /* Test two simultaneous receivers. */
    chan ch4 = chmake(int, 0);
    go(receiver(chdup(ch4), 333));
    go(receiver(chdup(ch4), 444));
    chs(ch4, int, 333);
    chs(ch4, int, 444);
    chclose(ch4);

    /* Test typed channels. */
    chan ch5 = chmake(char, 0);
    go(charsender(chdup(ch5), 111));
    char charval = chr(ch5, char);
    assert(charval == 111);
    chclose(ch5);
    chan ch6 = chmake(struct foo, 0);
    struct foo foo1 = {555, 222};
    go(structsender(chdup(ch6), foo1));
    struct foo foo2 = chr(ch6, struct foo);
    assert(foo2.first == 555 && foo2.second == 222);
    chclose(ch6);

    /* Test buffering. */
    chan ch7 = chmake(int, 2);
    chs(ch7, int, 222);
    chs(ch7, int, 333);
    val = chr(ch7, int);
    assert(val == 222);
    val = chr(ch7, int);
    assert(val == 333);
    chs(ch7, int, 444);
    val = chr(ch7, int);
    assert(val == 444);
    chs(ch7, int, 555);
    chs(ch7, int, 666);
    val = chr(ch7, int);
    assert(val == 555);
    val = chr(ch7, int);
    assert(val == 666);
    chclose(ch7);

    return 0;
}

