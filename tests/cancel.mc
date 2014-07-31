
#include <assert.h>

#include "../stdmill.h"

coroutine block ()
{
    go msleep (0, 1000000);
    select {
    case msleep:
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
}

