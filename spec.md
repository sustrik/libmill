Mill summary
============

Mill is a preprocessor that adds coroutine support to C. It is also
an experiment in adding more structure (think gotos vs. structured programming!)
into parallel programming.

*coroutine* keyword provides a way to define a new coroutine. The syntax mimics
the syntax of standard C function, except that it has not return value.

```
coroutine foo (int a, const char *b, int *result)
{
    *result = a + b;
}
```

To keep the preprocessor simple, *endvars* keyword is introduced to separate
local variable declarations in the coroutine from the executable code:

```
coroutine foo ()
{
    int i;
    endvars;

    i = 0;
}
```

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

Use *go* keyword to launch a coroutine.

```
coroutine foo ()
{
    printf ("Hello, world!\n");
}

coroutine bar ()
{
    go foo();
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

Note that events are distinguished based on coroutine types, not coroutine
instances. This is an experiment that seems to lead to better coding practices.

Coroutines can be invoked in synchronous manner from standard C functions:

```
coroutine foo ()
{
}

int main ()
{
    foo ();
    return 0;
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

Preprocessor command line: Following commands will generate foo.h from foo.mh
and foo.c from foo.mc:

```
mill foo.mh
mill foo.mc
```

