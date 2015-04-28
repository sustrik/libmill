
#include <assert.h>
#include <stdio.h>
#include "../mill.h"

int sum = 0;

void worker(int count, int n) {
    int i;
    for(i = 0; i != count; ++i) {
        sum += n;
        yield();
    }
}

int main() {
    go(worker(3, 7));
    go(worker(1, 11));
    go(worker(2, 5));
    musleep(100000);
    assert(sum == 42);
    return 0;
}

