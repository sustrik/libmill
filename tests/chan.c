
#include <stdio.h>
#include <assert.h>
#include "../mill.h"

void foo(chan ch) {
    //musleep(1000000);
yield();
    chs(ch, (void*)333);
    chclose(ch); 
}

int main() {
    chan ch = chmake();
    go(foo(ch));
    void *val = chr(ch);
    assert(val == (void*)333);
    return 0;
}

