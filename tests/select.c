
#include <stdio.h>
#include <assert.h>
#include "../mill.c"

int main() {
    chan ch1 = chmake();
    chan ch2 = chmake();
    chs(ch2, NULL);

    select {
    in(ch1):
        assert(0);
    in(ch2):
        printf("ch2 was selected\n");
    end
    }

    return 0;
}

