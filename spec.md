
To address the problem of multiple coroutines using the same location for the
output argument in parallel, *out* keyword is introduced. The argument is
allocated at callee's coframe (like a stack frame, but for coroutines) and
copied to the caller-specified destination immediately before coroutine
termination event is processed by the caller.

```
coroutine foo (out int result)
{
    result = 1;
}

coroutine bar ()
{
    int i;
    endvars;

    go foo (&i);
    go foo (&i);
    go foo (&i);
}
``` 

Note that all the coroutines launched from the current coroutine are canceled
automatically once the parent coroutine exits. Therefore, program's 'costack'
(as call stack, but for coroutines) forms a tree. There's no lifetime overlap
among the coroutines, callee's lifetime is fully contained within caller's
lifetime.

Use *select* keyword to wait for events. The only events supported by mill are
'done' events from the child coroutines and 'cancel' event from the parent.

```
coroutine foo ()
{
}

coroutine bar ()
{
    go foo ()
    select {
    case foo:
        printf ("foo is done\n");
    cancel:
        printf ("parent coroutine asked bar() to cancel itself\n");
        return;
    }
}
```

Finally, mill provides a library of atomic coroutines that can be used to build
more complex programs. These include sleeping, using sockets, DNS queries etc.

```
#include <stdmill.h>

coroutine foo ()
{
    go msleep (1000);
    select {
    case msleep:
        printf ("1 second elapsed\n");
    cancel:
        return;
    }
}
```

