MILL
====

This project is trying to introduce Go-style concurrency to C.

test.c:
```
#include <stdio.h>
#include "mill.h"

void worker(int count, const char *test) {
    int i;
    for(i = 0; i != count; ++i) {
        printf("%s\n", test);
        musleep(10000);
    }
}

int main() {
    go(worker(3, "a"));
    go(worker(1, "b"));
    go(worker(2, "c"));
    musleep(50000);
    return 0;
}
```

To build the test above:
```
$ gcc -o test test.c mill.c
```

This is a proof of concept project that seems to work with x86-64, gcc
and Linux. I have no idea about different environments. Also, the project
is in very early stage of development and not suitable for actual usage.
