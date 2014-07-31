
#include <assert.h>

#include "../stdmill.h"

coroutine fx1 (out int idout, int id, int milliseconds)
{
    go msleep (0, milliseconds);
    select {
    case msleep:
    }
    idout = id;
}

coroutine fx2 = fx1;

coroutine test ()
{
    int id;
    endvars;

    go fx1 (&id, 1, 100);
    go fx2 (&id, 2, 200);
    select {
    case fx2:
        assert (id == 2);
    }
    select {
    case fx1:
        assert (id == 1);
    }
}

int main ()
{
    _mill_trace ();
    test();
}

