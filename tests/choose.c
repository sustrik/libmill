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

#include "../libmill.h"

coroutine void sender1(chan ch, int val) {
    chs(ch, int, val);
    chclose(ch);
}

coroutine void sender2(chan ch, int val) {
    yield();
    chs(ch, int, val);
    chclose(ch);
}

coroutine void sender3(chan ch, int val, int64_t deadline) {
    msleep(deadline);
    chs(ch, int, val);
    chclose(ch);
}

coroutine void receiver1(chan ch, int expected) {
    int val = chr(ch, int);
    assert(val == expected);
    chclose(ch);
}

coroutine void receiver2(chan ch, int expected) {
    yield();
    int val = chr(ch, int);
    assert(val == expected);
    chclose(ch);
}

coroutine void choosesender(chan ch, int val) {
    choose {
    out(ch, int, val):
    end
    }
    chclose(ch);
}

coroutine void feeder(chan ch, int val) {
    while(1) {
        chs(ch, int, val);
        yield();
    }
}

coroutine void feeder2(chan ch, int first, int second) {
    while(1) {
        choose {
        out(ch, int, first):
        out(ch, int, second):
        end
        }
    }
}

coroutine void feeder3(chan ch, int val) {
    while(1) {
        msleep(10);
        chs(ch, int, val);
    }
}

coroutine void feeder4(chan ch) {
    while(1) {
        choose {
        out(ch, int, 1):
        out(ch, int, 2):
        out(ch, int, 3):
        end
        }
    }
}

struct large {
    char buf[1024];
};

/* Test whether choose with no clauses whatsoever compiles. */
coroutine void unused(void) {
    choose {
    end
    }
}

int main() {
    /* In this test we are also going to make sure that the debugging support
       doesn't crash the application. */
    gotrace(1);

    /* Non-blocking receiver case. */
    chan ch1 = chmake(int, 0);
    go(sender1(chdup(ch1), 555));
    choose {
    in(ch1, int, val):
        assert(val == 555);
    end
    }
    chclose(ch1);

    /* Blocking receiver case. */
    chan ch2 = chmake(int, 0);
    go(sender2(chdup(ch2), 666));
    choose {
    in(ch2, int, val):
        assert(val == 666);
    end
    }
    chclose(ch2);

    /* Non-blocking sender case. */
    chan ch3 = chmake(int, 0);
    go(receiver1(chdup(ch3), 777));
    choose {
    out(ch3, int, 777):
    end
    }
    chclose(ch3);

    /* Blocking sender case. */
    chan ch4 = chmake(int, 0);
    go(receiver2(chdup(ch4), 888));
    choose {
    out(ch4, int, 888):
    end
    }
    chclose(ch4);

    /* Check with two channels. */
    chan ch5 = chmake(int, 0);
    chan ch6 = chmake(int, 0);
    go(sender1(chdup(ch6), 555));
    choose {
    in(ch5, int, val):
        assert(0);
    in(ch6, int, val):
        assert(val == 555);
    end
    }
    go(sender2(chdup(ch5), 666));
    choose {
    in(ch5, int, val):
        assert(val == 666);
    in(ch6, int, val):
        assert(0);
    end
    }
    chclose(ch5);
    chclose(ch6);

    /* Test whether selection of in channels is random. */
    chan ch7 = chmake(int, 0);
    chan ch8 = chmake(int, 0);
    go(feeder(chdup(ch7), 111));
    go(feeder(chdup(ch8), 222));
    int i;
    int first = 0;
    int second = 0;
    int third = 0;
    for(i = 0; i != 100; ++i) {
        choose {
        in(ch7, int, val):
            assert(val == 111);
            ++first;
        in(ch8, int, val):
            assert(val == 222);
            ++second;
        end
        }
        yield();
    }
    assert(first > 1 && second > 1);
    chclose(ch7);
    chclose(ch8);

    /* Test 'otherwise' clause. */
    int test = 0;
    chan ch9 = chmake(int, 0);
    choose {
    in(ch9, int, val):
        assert(0);
    otherwise:
        test = 1;
    end
    }
    assert(test == 1);
    chclose(ch9);
    test = 0;
    choose {
    otherwise:
        test = 1;
    end
    }
    assert(test == 1);

    /* Test two simultaneous senders vs. choose statement. */
    int val;
    chan ch10 = chmake(int, 0);
    go(sender1(chdup(ch10), 888));
    go(sender1(chdup(ch10), 999));
    val = 0;
    choose {
    in(ch10, int, v):
        val = v;
    end
    }
    assert(val == 888);
    val = 0;
    choose {
    in(ch10, int, v):
        val = v;
    end
    }
    assert(val == 999);
    chclose(ch10);

    /* Test two simultaneous receivers vs. choose statement. */
    chan ch11 = chmake(int, 0);
    go(receiver1(chdup(ch11), 333));
    go(receiver1(chdup(ch11), 444));
    choose {
    out(ch11, int, 333):
    end
    }
    choose {
    out(ch11, int, 444):
    end
    }
    chclose(ch11);

    /* Choose vs. choose. */
    chan ch12 = chmake(int, 0);
    go(choosesender(chdup(ch12), 111));
    choose {
    in(ch12, int, v):
        assert(v == 111);
    end
    }
    chclose(ch12);

    /* Choose vs. buffered channels. */
    chan ch13 = chmake(int, 2);
    choose {
    out(ch13, int, 999):
    end
    }
    choose {
    in(ch13, int, val):
        assert(val == 999);
    end
    }
    chclose(ch13);

    /* Test whether selection of out channels is random. */
    chan ch14 = chmake(int, 0);
    go(feeder2(chdup(ch14), 666, 777));
    first = 0;
    second = 0;
    for(i = 0; i != 100; ++i) {
        int v = chr(ch14, int);
        if(v == 666)
            ++first;
        else if(v == 777)
            ++second;
        else
            assert(0);
    }
    assert(first > 1 && second > 1);
    chclose(ch14);

    /* Test whether allocating larger in buffer breaks previous in clause. */
    chan ch15 = chmake(struct large, 1);
    chan ch16 = chmake(int, 1);
    go(sender2(chdup(ch16), 1111));
    goredump();
    choose {
    in(ch16, int, val):
        assert(val == 1111);
    in(ch15, struct large, val):
        assert(0);
    end
    }
    chclose(ch16);
    chclose(ch15);

    /* Test transferring a large object. */
    chan ch17 = chmake(struct large, 1);
    struct large large = {{0}};
    chs(ch17, struct large, large);
    choose {
    in(ch17, struct large, v):
    end
    }
    chclose(ch17);

    /* Test that 'in' on done-with channel fires. */
    chan ch18 = chmake(int, 0);
    chdone(ch18, int, 2222);
    choose {
    in(ch18, int, val):
        assert(val == 2222);
    end
    }
    chclose(ch18);

    /* Test whether selection of in channels is random when there's nothing
       immediately available to receive. */
    first = 0;
    second = 0;
    third = 0;
    chan ch19 = chmake(int, 0);
    go(feeder3(chdup(ch19), 3333));
    for(i = 0; i != 100; ++i) {
        choose {
        in(ch19, int, val):
            ++first;
        in(ch19, int, val):
            ++second;
        in(ch19, int, val):
            ++third;
        end
        }
    }
    assert(first > 1 && second > 1 && third > 1);
    chclose(ch19);

    /* Test whether selection of out channels is random when sending
       cannot be performed immediately. */
    first = 0;
    second = 0;
    third = 0;
    chan ch20 = chmake(int, 0);
    go(feeder4(chdup(ch20)));
    for(i = 0; i != 100; ++i) {
        msleep(10);
        val = chr(ch20, int);
        switch(val) {
        case 1:
            ++first;
            break;
        case 2:
            ++second;
            break;
        case 3:
            ++third;
            break;
        default:
            assert(0);
        }
    }
    assert(first > 1 && second > 1 && third > 1);
    chclose(ch20);

    /* Test expiration of 'deadline' clause. */
    test = 0;
    chan ch21 = chmake(int, 0);
    int64_t start = now();
    choose {
    in(ch21, int, val):
        assert(0);
    deadline(start + 50):
        test = 1;
    end
    }
    assert(test == 1);
    int64_t diff = now() - start;
    assert(diff > 30 && diff < 70);
    chclose(ch21);

    /* Test unexpired 'deadline' clause. */
    test = 0;
    chan ch22 = chmake(int, 0);
    start = now();
    go(sender3(chdup(ch22), 4444, start + 50));
    choose {
    in(ch22, int, val):
        test = val;
    deadline(start + 1000):
        assert(0);
    end
    }
    assert(test == 4444);
    diff = now() - start;
    assert(diff > 30 && diff < 70);
    chclose(ch22);

    return 0;
}

