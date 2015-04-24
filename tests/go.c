
#include <stdio.h>
#include "../mill.h"

void worker(int count, const char *test) {
    int i;
    for(i = 0; i != count; ++i) {
        printf("%s\n", test);
        yield();
    }
}

int main() {
    go(worker(3, "a"));
    go(worker(1, "b"));
    go(worker(2, "c"));
    worker(5,"d");
    return 0;
}

