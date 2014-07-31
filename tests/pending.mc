
#include <assert.h>

#include "../stdmill.h"

coroutine fx1 (out int idout, int id, int milliseconds)
{
    int rc;
    endvars;

    go msleep (&rc, milliseconds);
    select {
    case msleep:
    }
    assert (rc == 0);
    idout = id;
}

coroutine test ()
{
    int id;
    endvars;

    go fx1 (&id, 1, 1000);
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

