
#include <assert.h>

#include "../stdmill.h"

int counter = 0;

coroutine block ()
{
    ++counter;
    go msleep (0, 1000000);
    select {
    case msleep:
    cancel:
        --counter;
    }
}

coroutine test ()
{
    int id;
    endvars;

    go block ();
}

int main ()
{
    _mill_trace ();
    test();
    assert (counter == 0);
}

