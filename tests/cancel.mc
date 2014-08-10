
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

coroutine nonblock()
{
    ++counter;
cancel:
    --counter;
}

coroutine test ()
{
    go block ();
    go nonblock ();
}

int main ()
{
    _mill_trace ();
    test();
    assert (counter == 0);
}

