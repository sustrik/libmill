
#include <stdio.h>
#include <assert.h>
#include "../mill.h"

void sender1(chan ch) {
    chs(ch, (void*)555);
    chclose(ch);
}

void sender2(chan ch) {
    yield();
    chs(ch, (void*)666);
    chclose(ch);
}

void receiver1(chan ch) {
    void *val = chr(ch);
    assert(val == (void*)777);
    chclose(ch);
}

void receiver2(chan ch) {
    yield();
    void *val = chr(ch);
    assert(val == (void*)888);
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
    go(sender1(ch1));
    choose {
    in(ch1, val):
        assert(val == (void*)555);
    end
    }
    chclose(ch1);

    /* Blocking receiver case. */
    chan ch2 = chmake();
    go(sender2(ch2));
    choose {
    in(ch2, val):
        assert(val == (void*)666);
    end
    }
    chclose(ch2);

    /* Non-blocking sender case. */
    chan ch3 = chmake();
    go(receiver1(ch3));
    choose {
    out(ch3, (void*)777):
    end
    }
    chclose(ch3);

    /* Blocking sender case. */
    chan ch4 = chmake();
    go(receiver2(ch4));
    choose {
    out(ch4, (void*)888):
    end
    }
    chclose(ch4);

    /* Check with two channels. */
    chan ch5 = chmake();
    chan ch6 = chmake();
    go(sender1(ch6));
    choose {
    in(ch5, val):
        assert(0);
    in(ch6, val):
        assert(val == (void*)555);
    end
    }
    go(sender2(ch5));
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
    go(feeder(ch7, (void*)111));
    go(feeder(ch8, (void*)222));
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
    chclose(ch9);

    return 0;
}

