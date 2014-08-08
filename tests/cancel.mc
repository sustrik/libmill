
#include <assert.h>
#include <stddef.h>

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

coroutine alloc(out void **result)
{
    ++counter;
cancel:
    --counter;
}

coroutine test ()
{
    go block ();
    go alloc (NULL);
}

int main ()
{
    _mill_trace ();
    test();
    assert (counter == 0);
}

