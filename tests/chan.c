
#include <stdio.h>
#include <assert.h>
#include "../mill.h"

void foo(chan ch) {
    chan_val val;
    val.i = 333;
    chan_send(ch, val);
    chan_close(ch); 
}

int main() {
    chan ch = chan_init();
    go(foo(ch));
    chan_val val;
    val.i = 0;
    int res = chan_select(&val, ch);
    assert(res == 0);
    assert(val.i == 333);
    return 0;
}

