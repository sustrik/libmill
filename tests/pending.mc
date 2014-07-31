
#include <assert.h>

#include "../stdmill.h"

coroutine fx1 (out int idout, int id, int milliseconds)
{
    int rc;
    endvars;

    msleep (&rc, milliseconds);
    select {
    case msleep:
    }
    assert (rc == 0);
    idout = id;
}

coroutine fx2 = fx1;

coroutine test ()
{
    int id;
    endvars;

    go fx1 (&id, 1, 1000);
    select {
    case msleep:
        assert (id == 1);
    }
    printf ("done\n");
}

int main ()
{
    test();
}
