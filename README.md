Mill
====

Mill is a preprocessor that adds coroutine support to C language. It is also
an experiment in adding more structure (think gotos vs. structured programming)
into parallel programming.

The result of preprocessing are standard C source and header files that can be
added directly to your C project.

The project also contains library of elementary coroutines (timers,
network I/O et c.)

WARNING: THIS PROJECT IS UNDER DEVELOPMENT! DO NOT USE YET!

## Prerequisites

* Install Ruby
* Install libuv from here: https://github.com/joyent/libuv

The former is needed at compilation time, the latter at both compilation time
and runtime.

## Example

```
#include <stdio.h>
#include <stddef.h>
#include <stdmill.h>

coroutine test ()
{
    go msleep (NULL, 1000);
    go msleep (NULL, 2000);

    while (1) {
        select {
        msleep:
            printf ("Timer elapsed!\n");
        }
    }
}

int main ()
{
    test ();
    return 0;
}
```

To run it:

```
$ mill test.mc
$ gcc -o test test.c -lstdmill
$ ./test
```

Note that the two 'msleep' coroutines are running in parallel with the main
thread!

## Documentation

### Introduction to coroutines

Coroutines are a way to implement cooperative multitasking in a rather simple
and developer-friendly manner.

If you are not familiar with coroutines, check the wikipedia article here:

https://en.wikipedia.org/wiki/Coroutine

A nice explanation of how to implement coroutines in C can be found here:

http://www.chiark.greenend.org.uk/~sgtatham/coroutines.html

### Mill

To use coroutines, it is best to choose a language that has direct support for
the concept, such as Go.

However, sometimes there is no other option but to use C. After all, C is
the lingua franca of programming languages and every other language as well
as every single OS integrates well with C.

Writing coroutines by hand (see the link above) is an option, but once you move
beyond the very basic functionality all the required boilerplate code is both
annoying to write and confusing to read.

Mill tries to solve this problem by defining a slightly augmented version of
C language, one with direct support for coroutines.

Mill is more than a coroutine generator though. It tries to tame the complexity
inherent in asynchronous programming by defining additional constrains on what
can be done. For example, the lifetime of coroutine must be fully contained in
the lifetime of the coroutine that launched it. In other words, when
coroutine ends, all the child coroutines are automatically canceled. Another
example: Each coroutine can communicate only with its parent coroutine and can
send it only a single "I am done" message.

The hope is that such constrains won't affect expressivity of the language,
yet, they will make the code cleaner, simpler and easier to understand and
maintain.

### Usage

Following commands will generate foo.h from foo.mh and foo.c from foo.mc:

```
mill foo.mh
mill foo.mc
```

### Defining a coroutine

Use "coroutine" keyword to define a new coroutine. The syntax mimics the syntax
of standard C function, except that it has no return value:

```
coroutine foo (int a, const char *b, int *result)
{
    *result = a + b;
}
```

Mill is a simple preprocessor that doesn't do full semantic analysis of the
source. The price for such simplicity is that it is not able to automatically
distinguish variable declarations from the rest of the codebase. To help the
parser, you must place all the local variable declarations to the beginning of
the coroutine followed by "endvars" keyword:

```
coroutine foo ()
{
    int i = 0;
    endvars;

    i = i + 1
}
```

WARNING: If you put local variable declarations elsewhere, mill won't complain
about it but the behaviour of the coroutine will become undefined. Expect values
of such variables to change randomly while coroutine is in progress.

### Coroutine aliasing

You can define new coroutine to have exactly the same behaviour as a different
coroutine:

```
coroutine foo = bar;
```

While the construct looks a bit superfluous, it is actually quite important
tool for keeping the codebase simple. Usage of the construct will be discussed
in subsequent sections.

### Launching coroutines

Use "go" keyword to launch a coroutine.

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

The coroutine will be executed in parallel with the parent coroutine.

Please note that coroutines can be launched in this way only from other
coroutines. If you want to launch a coroutine from standard C function you
have to do so in synchornous manner:

```
coroutine foo ()
{
    printf ("Hello, world!\n");
}

int main ()
{
    foo ();
    return 0;
}
```

In the latter case the coroutine won't execute in parallel with the parent 
function. Instead, the function will be block until the execution of the
coroutine have finished.

### Waiting for coroutines

Use "select" keyword to wait for termination of child coroutines:

```
coroutine foo ()
{
}

coroutine bar ()
{
    go foo ();
    select {
    case foo:
        printf ("foo is done\n");
    }
}
```

Note that you are waiting for a specific *type* of coroutine rather than for
specific coroutine invocation. Thus, in the following example "select" waits
for termination of any of the child coroutines:

```
coroutine bar ()
{
    go foo ();
    go foo ();
    go foo ();

    while (1) {
        select {
        case foo:
            printf ("foo is done!\n");
        }
    }
}
```

## License

Mill is licensed under MIT/X11 license.
