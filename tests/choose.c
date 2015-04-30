
#include <stdio.h>
#include <assert.h>
#include "../mill.h"

void sender1(chan ch, void *val) {
    chs(ch, val);
    chclose(ch);
}

void sender2(chan ch, void *val) {
    yield();
    chs(ch, val);
    chclose(ch);
}

void receiver1(chan ch, void *expected) {
    void *val = chr(ch);
    assert(val == expected);
    chclose(ch);
}

void receiver2(chan ch, void *expected) {
    yield();
    void *val = chr(ch);
    assert(val == expected);
    chclose(ch);
}

void feeder(chan ch, void *val) {
    while(1) {
        chs(ch, val);
        yield();
    }
}

int main() {
    /* Non-blocking receiver case. */
    chan ch1 = chmake();
    go(sender1(chdup(ch1), (void*)555));
    choose {
    in(ch1, val):
        assert(val == (void*)555);
    end
    }
    chclose(ch1);

    /* Blocking receiver case. */
    chan ch2 = chmake();
    go(sender2(chdup(ch2), (void*)666));
    choose {
    in(ch2, val):
        assert(val == (void*)666);
    end
    }
    chclose(ch2);

    /* Non-blocking sender case. */
    chan ch3 = chmake();
    go(receiver1(chdup(ch3), (void*)777));
    choose {
    out(ch3, (void*)777):
    end
    }
    chclose(ch3);

    /* Blocking sender case. */
    chan ch4 = chmake();
    go(receiver2(chdup(ch4), (void*)888));
    choose {
    out(ch4, (void*)888):
    end
    }
    chclose(ch4);

    /* Check with two channels. */
    chan ch5 = chmake();
    chan ch6 = chmake();
    go(sender1(chdup(ch6), (void*)555));
    choose {
    in(ch5, val):
        assert(0);
    in(ch6, val):
        assert(val == (void*)555);
    end
    }
    go(sender2(chdup(ch5), (void*)666));
    choose {
    in(ch5, val):
        assert(val == (void*)666);
    in(ch6, val):
        assert(0);
    end
    }
    chclose(ch5);
    chclose(ch6);

    /* Test whether selection of channels is random. */
    chan ch7 = chmake();
    chan ch8 = chmake();
    go(feeder(chdup(ch7), (void*)111));
    go(feeder(chdup(ch8), (void*)222));
    int i;
    int first = 0;
    int second = 0;
    for(i = 0; i != 100; ++i) {
        choose {
        in(ch7, val):
            assert(val == (void*)111);
            ++first;
        in(ch8, val):
            assert(val == (void*)222);
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
    chan ch9 = chmake();
    choose {
    in(ch9, val):
        assert(0);
    otherwise:
        test = 1;
    end
    }
    assert(test == 1);
    chclose(ch9);

    /* Test two simultaneous senders vs. choose statement. */
    chan ch11 = chmake();
    go(sender1(chdup(ch11), (void*)888));
    go(sender1(chdup(ch11), (void*)999));
    val = NULL;
    choose {
    in(ch11, v):
        val = v;
    end
    }
    assert(val == (void*)888);
    val = NULL;
    choose {
    in(ch11, v):
        val = v;
    end
    }
    assert(val == (void*)999);
    chclose(ch11);

    /* Test two simultaneous receivers vs. choose statement. */
    chan ch13 = chmake();
    go(receiver1(chdup(ch13), (void*)333));
    go(receiver1(chdup(ch13), (void*)444));
    choose {
    out(ch13, (void*)333):
    end
    }
    choose {
    out(ch13, (void*)444):
    end
    }
    chclose(ch11);

    return 0;
}

