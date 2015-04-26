
#include <stdio.h>
#include <assert.h>
#include "../mill.h"

void sender1(chan ch) {
    chs(ch, (void*)555);
}

void sender2(chan ch) {
    yield();
    chs(ch, (void*)666);
}

int main() {
    /* Non-blocking case. */
    chan ch1 = chmake();
    go(sender1(ch1));
    chselect {
    in(ch1, val):
        assert(val == (void*)555);
    end
    }

    /* Blocking case. */
    chan ch2 = chmake();
    go(sender2(ch2));
    chselect {
    in(ch2, val):
        assert(val == (void*)666);
    end
    }

    return 0;
}

