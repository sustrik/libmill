
#include <assert.h>

#include "../stdmill.h"

coroutine fx1 (out int idout, int id, int milliseconds)
{
    int rc;
    endvars;

    msleep (&rc, milliseconds);
    assert (rc == 0);
    idout = id;
}

coroutine fx2 = fx1;

coroutine test ()
{
    int id;
    int rc;
    endvars;

    /* This coroutines will be waited for. */
    go fx2 (&id, 1, 40);
    go fx1 (&id, 2, 20);
    go fx2 (&id, 3, 50);
    go fx1 (&id, 4, 30);
    go fx1 (&id, 5, 100);
    go fx2 (&id, 6, 10);

    /* These coroutines will be canceled. */
    go msleep (&rc, 10);
    go msleep (&rc, 150);

    select {
    case fx1:
        assert (id == 2);
    }
    select {
    case fx1:
        assert (id == 4);
    }
    select {
    case fx1:
        assert (id == 5);
    }
    select {
    case fx2:
        assert (id == 6);
    }
    select {
    case fx2:
        assert (id == 3);
    }
    select {
    case fx2:
        assert (id == 1);
    }
}

int main ()
{
    test();
}
