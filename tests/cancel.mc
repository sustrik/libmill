
#include <assert.h>

#include "../stdmill.h"

coroutine pause ()
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

    go pause ();
}

int main ()
{
    _mill_trace ();
    test();
}

