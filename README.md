Mill
====

Mill is a preprocessor that adds coroutine support to C language.

The result of preprocessing are standard C source and header files that can be
added directly to your C projects.

WARNING: THIS PROJECT IS UNDER DEVELOPMENT! DO NOT USE YET!

## Prerequisites

* Install Ruby
* Install libuv from here: https://github.com/joyent/libuv

The former is needed at compilation time, the latter at both compilation time
and runtime.

## Example

```
#include <stdio.h>
#include <msleep.h>

coroutine stopwatch ()
{
    void *s1;
    void *s2;
    endvars;

    s1 = call msleep (1000);
    s2 = call msleep (2000);

    while (1) {
        wait;
        if (@who == s1)
            printf ("1 second elapsed!\n");
        if (@who == s2)
            printf ("2 seconds elapsed!\n");
    }
}

int main ()
{
    stopwatch ();
    return 0;
}
```

Note that the two 'msleep' coroutines are run in parallel with the main thread!

## Usage

```
mill stopwatch.mill
gcc -o stopwatch stopwatch.c stopwatch.c -luv
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

It is important for coroutines to not perform any blocking operations.
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
on with the processing. In fact, invoking a coroutine synchronously almost the
same as invoking a classic C function and even the syntax looks the same:

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

# Invoke a coroutine asynchronously

Asynchronous invocation of coroutines uses 'call' keyword and can be done only
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

## License

Mill is licensed under MIT/X11 license.
