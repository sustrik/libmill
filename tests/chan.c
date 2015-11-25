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
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>

#include "../libmill.h"

struct foo {
    int first;
    int second;
};

coroutine void sender(chan ch, int doyield, int val) {
    if(doyield)
        yield();
    chs(ch, int, val);
    chclose(ch);
}

coroutine void receiver(chan ch, int expected) {
    int val = chr(ch, int);
    assert(val == expected);
    chclose(ch);
}

coroutine void receiver2(chan ch, int expected, chan back) {
    int val = chr(ch, int);
    assert(val == expected);
    chclose(ch);
    chs(back, int, 0);
    chclose(back);
}

coroutine void charsender(chan ch, char val) {
    chs(ch, char, val);
    chclose(ch);
}

coroutine void structsender(chan ch, struct foo val) {
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
    yield();
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

    /* Test message buffering. */
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

    /* Test simple chdone() scenarios. */
    chan ch8 = chmake(int, 0);
    chdone(ch8, int, 777);
    val = chr(ch8, int);
    assert(val == 777);
    val = chr(ch8, int);
    assert(val == 777);
    val = chr(ch8, int);
    assert(val == 777);
    chclose(ch8);
    chan ch9 = chmake(int, 10);
    chdone(ch9, int, 888);
    val = chr(ch9, int);
    assert(val == 888);
    val = chr(ch9, int);
    assert(val == 888);
    chclose(ch9);
    chan ch10 = chmake(int, 10);
    chs(ch10, int, 999);
    chdone(ch10, int, 111);
    val = chr(ch10, int);
    assert(val == 999);
    val = chr(ch10, int);
    assert(val == 111);
    val = chr(ch10, int);
    assert(val == 111);
    chclose(ch10);
    chan ch11 = chmake(int, 1);
    chs(ch11, int, 222);
    chdone(ch11, int, 333);
    val = chr(ch11, int);
    assert(val == 222);
    val = chr(ch11, int);
    assert(val == 333);
    chclose(ch11);

    /* Test whether chdone() unblocks all receivers. */
    chan ch12 = chmake(int, 0);
    chan ch13 = chmake(int, 0);
    go(receiver2(chdup(ch12), 444, chdup(ch13)));
    go(receiver2(chdup(ch12), 444, chdup(ch13)));
    chdone(ch12, int, 444);
    val = chr(ch13, int);
    assert(val == 0);
    val = chr(ch13, int);
    assert(val == 0);
    chclose(ch13);
    chclose(ch12);

    /* Test a combination of blocked sender and an item in the channel. */
    chan ch14 = chmake(int, 1);
    chs(ch14, int, 1);
    go(sender(chdup(ch14), 0, 2));
    val = chr(ch14, int);
    assert(val == 1);
    val = chr(ch14, int);
    assert(val == 2);
    chclose(ch14);

    return 0;
}

