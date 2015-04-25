
#include <stdio.h>
#include <assert.h>
#include "../mill.h"

void sender(chan ch, int doyield, void *val) {
    if(doyield)
        yield();
    chs(ch, val);
    chclose(ch); 
}

int main() {
    chan ch;

    /* Receiver waits for sender. */
    ch = chmake();
    go(sender(ch, 1, (void*)333));
    assert(chr(ch) == (void*)333);
    chclose(ch);

    /* Sender waits for receiver. */
    ch = chmake();
    go(sender(ch, 0, (void*)444));
    assert(chr(ch) == (void*)444);
    chclose(ch);

    return 0;
}

