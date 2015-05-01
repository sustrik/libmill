
#include <assert.h>
#include <stdio.h>
#include "../mill.h"

void sender(chan ch, int doyield, int val) {
    if(doyield)
        yield();
    chs(ch, int, val);
    chclose(ch); 
}

void sender1(chan ch, int val) {
    chs(ch, int, val);
    chclose(ch);
}

void receiver1(chan ch, int expected) {
    int val = chr(ch, int);
    assert(val == expected);
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
    go(sender1(chdup(ch3), 888));
    go(sender1(chdup(ch3), 999));
    val = chr(ch3, int);
    assert(val == 888);
    val = chr(ch3, int);
    assert(val == 999);
    chclose(ch3);

    /* Test two simultaneous receivers. */
    chan ch4 = chmake(int, 0);
    go(receiver1(chdup(ch4), 333));
    go(receiver1(chdup(ch4), 444));
    chs(ch4, int, 333);
    chs(ch4, int, 444);
    chclose(ch4);

    return 0;
}

