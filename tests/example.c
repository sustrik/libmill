
#include <stdio.h>
#include "../mill.h"

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

    chan ch1 = chmake(int);
    go(worker(4, "a", chdup(ch1)));
    chan ch2 = chmake(int);
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

