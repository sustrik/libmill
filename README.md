MILL
====

This project is trying to introduce Go-style concurrency to C.

test.c:
```c
#include <stdio.h>
#include <mill.h>

void worker(int count, const char *text, chan ch) {
    int i;
    for(i = 0; i != count; ++i) {
        printf("%s\n", text);
        musleep(10000);
    }
    chs(ch, int, 0);
    chclose(ch);
}

int main() {

    chan ch1 = chmake(int, 0);
    go(worker(4, "a", chdup(ch1)));
    chan ch2 = chmake(int, 0);
    go(worker(2, "b", chdup(ch2)));

    choose {
    in(ch1, int, val):
        printf("coroutine 'a' have finished first!\n");
    in(ch2, int, val):
        printf("coroutine 'b' have finished first!\n");
    end
    }

    chclose(ch2);
    chclose(ch1);

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

Mailing List
------------

Discussion goes on at millc@freelists.org

Subscribe here:

http://www.freelists.org/list/millc

Archives:

http://www.freelists.org/archive/millc/
