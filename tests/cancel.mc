
#include <assert.h>

#include <stdmill.mh>

int counter = 0;

coroutine block ()
{
    ++counter;
    go msleep (1000000);
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
    test();
    assert (counter == 0);
}

