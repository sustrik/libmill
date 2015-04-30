
#include <stdio.h>
#include <assert.h>
#include "../mill.h"

void sender(chan ch, int doyield, void *val) {
    if(doyield)
        yield();
    chs(ch, val);
    chclose(ch); 
}

void sender1(chan ch, void *val) {
    chs(ch, val);
    chclose(ch);
}

void receiver1(chan ch, void *expected) {
    void *val = chr(ch);
    assert(val == expected);
    chclose(ch);
}

int main() {
    /* Receiver waits for sender. */
    chan ch1 = chmake();
    go(sender(chdup(ch1), 1, (void*)333));
    assert(chr(ch1) == (void*)333);
    chclose(ch1);

    /* Sender waits for receiver. */
    chan ch2 = chmake();
    go(sender(chdup(ch2), 0, (void*)444));
    assert(chr(ch2) == (void*)444);
    chclose(ch2);

    /* Test two simultaneous senders. */
    void *val;
    chan ch3 = chmake();
    go(sender1(chdup(ch3), (void*)888));
    go(sender1(chdup(ch3), (void*)999));
    val = chr(ch3);
    assert(val == (void*)888);
    val = chr(ch3);
    assert(val == (void*)999);
    chclose(ch3);

    /* Test two simultaneous receivers. */
    chan ch4 = chmake();
    go(receiver1(chdup(ch4), (void*)333));
    go(receiver1(chdup(ch4), (void*)444));
    chs(ch4, (void*)333);
    chs(ch4, (void*)444);
    chclose(ch4);

    return 0;
}

