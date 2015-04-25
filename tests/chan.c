
#include <stdio.h>
#include <assert.h>
#include "../mill.h"

void foo(chan ch) {
    yield();
    chs(ch, (void*)333);
    chclose(ch); 
}

int main() {
    chan ch = chmake();
    go(foo(ch));
    void *val = chr(ch);
    assert(val == (void*)333);
    chclose(ch);
    return 0;
}

