
This project is trying to introduce Go-style concurrency to C.

To build the library:
```
$ ./autogen.sh
$ ./configure
$ make
$ sudo make install
```

test.c:
```
#include <stdio.h>
#include <mill.h>

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
$ gcc -o test test.c -lmill
```

The project is in early stage of development and not suitable for actual usage.
