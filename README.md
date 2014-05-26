Mill
====

Mill is a preprocessor that adds coroutine support to C language.

The result of preprocessing are standard C source and header files that can be
added directly to your C project.

WARNING: THIS PROJECT IS UNDER DEVELOPMENT! DO NOT USE YET!

## Prerequisites

* Install Ruby
* Install libuv from here: https://github.com/joyent/libuv

The former is needed at compilation time, the latter at both compilation time
and runtime.

## Example

```
#include <stdio.h>

coroutine stopwatch ()
{
    void *sleep1;
    void *sleep2;
    void *hndl;
    endvars;

    sleep1 = call msleep (1000);
    sleep2 = call msleep (2000);

    while (1) {
        wait (&hndl, 0);
        if (hndl == sleep1)
            printf ("1 second elapsed!\n");
        if (hndl == sleep2)
            printf ("2 seconds elapsed!\n");
    }
}

int main ()
{
    stopwatch ();
    return 0;
}
```

Note that the two 'msleep' coroutines are running in parallel with the main
thread!

## Usage

```
mill stopwatch.mill
gcc -o stopwatch stopwatch.c mill.c -luv
./stopwatch
```

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
the lingua franca of programming languages and every single language as well
as every single OS integrates well with C.

Writing coroutines by hand (see the link above) is an option, but once you move
beyond the very basic functionality all the required boilerplate code is both
annoying to write and confusing to read.

Mill tries to solve this problem by defining a slightly augmented version of
C language, one with direct support for coroutines.

### Defining a coroutine

To define a new coroutine use "coroutine" keyword. The coroutine definition
is similar to a standard C function definition. One difference to note though
is that mill coroutines don't have a return types. From C point of view they
can be though of as void functions:

```
#include <stdio.h>

coroutine hello ()
{
    printf ("Hello, world!");
}
```

It is important for coroutines not to perform any blocking operations.
The nature of cooperative multitasking is such that a single blocked coroutine
can block all the other simultaneously running coroutines.

The following coroutine, for example, will block the entire program for an hour:

```
#include <unistd.h>

coroutine foo ()
{
    sleep (3600);
}
```

To perform blocking operations, such as sleeping or reading from a socket, you
should use asynchronous coroutines rather than classic C blocking functions.

Conveniently, mill provides a library of atomic coroutines to use. These will
be discussed in detail later on in this guide.

### Invoke a coroutine synchronously

Coroutine can be invoked in either synchronous or asynchronous manner.

In the former case the caller waits for the coroutine to finish before moving
on. In fact, invoking a coroutine synchronously almost the same as invoking
a classic C function and even the syntax looks the same:

```
#include <stdio.h>

coroutine hello ()
{
    printf ("Hello, world!");
}

int main ()
{
    hello ();
    return 0;
}
```

One thing to keep in mind is that synchronous invocation of a coroutine is
typically a blocking operation and thus should not be done from within
coroutines.

The main use for synchronous invocation of coroutines is integrating the
coroutines with the raw C code. So, for example, in the code snippet above
the coroutine is invoked synchronously from a classic C function (main).

### Invoke a coroutine asynchronously

Asynchronous invocation of a coroutine uses 'call' keyword and can be done only
from within another coroutine:

```
#include <stdio.h>

coroutine hello ()
{
    printf ("Hello, world!");
}

coroutine foo ()
{
    call hello ();
}
```

### Wait for completion of asynchronous coroutines

'wait' keyword can be used to wait for completion of any asynchronous coroutine
invoked from the current coroutine. Following example uses standard 'msleep'
coroutine provided by mill:

```
#include <msleep.h>

coroutine quux ()
{
    call msleep (1000);
    wait (0, 0);
}
```

Note that 'wait' although it looks like a blocking operation, is actually a
context switch that allows other coroutines to take the control of the CPU
in case there are no child coroutines finished.

### Coroutine handles

Given that in the example above there's only a single coroutine invoked,
the 'wait' will simply wait till it finishes.

However, more often than not you have multiple child coroutines running in
parallel. In such case you need a way to find out which of them has finished.

To do so, you can retrieve a coroutine handle from 'call' and later on use
to compare it to the handles retreived by 'wait' function:

```
#include <msleep.h>
#include <assert.h>

void *s;
void *hndl;

coroutine quux ()
{
    s = call msleep (1000);
    wait (&hndl, 0);
    assert (@hndl == handle);
}
```

### Local variables in coroutines

To keep the mill preprocessor simple, there are some restrictions for using
local variables in coroutines.

First , all local variables should be defined at the beginning of the
coroutine.

Second, local variable declarations should be followed by 'endvars' statement:

```
coroutine quux (int a)
{
    int b;
    endvars;

    b = 3;
    printf ("%d\n", a + b);
}
```

## Reference

### Defining coroutines

1. coroutine name { ... }
2. coroutine name : base { ... }
3. endvars;
4. void raise (int err);
5. finally

### Invoking coroutines

1. void *call name ( ... );
2. void *call (void *coframe) name ( ... );

### Waiting for coroutines

1. wait (void **hndl, int *err);
2. void *typeof (void *hndl);
3. void *typeof (name)

### Cancelling coroutines

1. void cancel (void *hndl);
2. void cancelall ();

## License

Mill is licensed under MIT/X11 license.
