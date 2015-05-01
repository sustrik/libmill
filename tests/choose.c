
#include <stdio.h>
#include <assert.h>
#include "../mill.h"

void sender1(chan ch, int val) {
    chs(ch, int, val);
    chclose(ch);
}

void sender2(chan ch, int val) {
    yield();
    chs(ch, int, val);
    chclose(ch);
}

void receiver1(chan ch, int expected) {
    int val = chr(ch, int);
    assert(val == expected);
    chclose(ch);
}

void receiver2(chan ch, int expected) {
    yield();
    int val = chr(ch, int);
    assert(val == expected);
    chclose(ch);
}

void choosesender(chan ch, int val) {
    choose {
    out(ch, int, val):
    end
    }
    chclose(ch);
}

void feeder(chan ch, int val) {
    while(1) {
        chs(ch, int, val);
        yield();
    }
}

int main() {
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

    /* Test whether selection of channels is random. */
    chan ch7 = chmake(int, 0);
    chan ch8 = chmake(int, 0);
    go(feeder(chdup(ch7), 111));
    go(feeder(chdup(ch8), 222));
    int i;
    int first = 0;
    int second = 0;
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

    return 0;
}

