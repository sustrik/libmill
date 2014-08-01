# Mill

Mill is a preprocessor that adds coroutine support to C language. It is also
an experiment in adding more structure (think gotos vs. structured programming)
into parallel programming.

The result of preprocessing are standard C source and header files that can be
added directly to your C project.

The project also contains library of elementary coroutines (timers,
network I/O et c.) that can be used to build more complex functionality.

WARNING: THIS PROJECT IS UNDER DEVELOPMENT! DO NOT USE YET!

## Prerequisites

* Install Ruby
* Install libuv from here: https://github.com/joyent/libuv

The former is needed at compilation time, the latter at both compilation time
and runtime.

## Installation

```
$ ./autogen.sh
$ ./configure
$ make
$ sudo make install
```

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
$ gcc -o test test.c -lstdmill -luv
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
the lingua franca of programming languages. Every other language integrates
well with C. C works on any operating system. Et c.

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
$ mill foo.mh
$ mill foo.mc
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
distinguish variable declarations from the other stuff in the codebase. To help
the parser, you must place all the local variable declarations to the beginning
of the coroutine. The declarations must be followed by "endvars" keyword:

```
coroutine foo ()
{
    int i = 0;
    endvars;

    i = i + 1
}
```

WARNING: If you put local variable declarations elsewhere, mill won't complain
about the fact but the behaviour of the coroutine will become undefined. Expect
values of such variables to change randomly while coroutine is in progress.

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

In this case the coroutine won't execute in parallel with the parent 
function. Instead, the function will be blocked until the coroutine is finished.

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

### Returning data from coroutines

Coroutines have no return values. However, you can use standard output
parameters and they should work as expected:

```
coroutine foo (int *result)
{
    *result = 1;
}

coroutine bar ()
{
    int i;
    endvars;

    go foo (&i);

    select {
    case foo:
        assert (i == 1);
    }
}
```

There's a problem though if multiple coroutines write the result into a single
location:

```
coroutine foo (int *result, int i)
{
    *result = i;
}

coroutine bar ()
{
    int i;
    endvars;

    go foo (&i, 1);
    go foo (&i, 2);

    select {

    // This clause may be invoked because foo(1) have ended, thus i == 1.
    case foo: 

        // foo(2) ends here and stores value 2 into i.
        assert (i == 1); // Assert fails.
    }
}
```

To address this problem "out" keyword is introduced. The coroutine argument
marked as "out" is allocated at callee's coframe (like a stack frame, but for
coroutines) and copied to the caller-specified destination immediately before
coroutine termination event is processed by the caller:

```
coroutine foo (out int result, int i)
{
    result = i;
}

coroutine bar ()
{
    int i;
    endvars;

    go foo (&i, 1);
    go foo (&i, 2);
    go foo (&i, 3);

    while (1) {
        select {
        case foo:
            printf ("coroutine foo returned %d\n", i);
        }
    }
}
```

Note that inside of the coroutine "result" argument is of type int, however,
the caller supplies argument of type int*.

### Comment on memory management

Keep in mind that coframes are deallocated when the child coroutine is
selected. If you launch a coroutine without selecting it afterwards, the
coframe will linger on in the memory until it is deallocated when the
caller coroutine finishes.

The behaviour is perfectly all right in most circumstances, however, doing
such thing in a loop will quickly exhaust the memory:

```
coroutine foo ()
{
    while (1) {
        go msleep (NULL, 1000);
    }
}
```

### Debugging

To make debugging of the asynchronous system easier, mill has support for
tracing. You can switch it on by invoking "_mill_trace();" function.

The tracing output looks like this:

```
mill ==> go     test
mill ==> go     test/fx1
mill ==> go     test/fx1/msleep
mill ==> go     test/fx2
mill ==> go     test/fx2/msleep
mill ==> return test/fx1/msleep
mill ==> select test/fx1/msleep
mill ==> cancel test/fx2
mill ==> cancel test/fx2/msleep
mill ==> return test/fx2/msleep
mill ==> return test/fx2
mill ==> return test
```

Each line contains an operation and the coroutine's backtrace. So, for example:

```
mill ==> return test/fx2
```

Means that coroutine "fx2" invoked from top-level coroutine "test" have finished
execution.

## License

Mill is licensed under MIT/X11 license.

