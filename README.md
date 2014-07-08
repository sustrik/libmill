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
#include <assert.h>
#include <stdmill.h>

coroutine stopwatch ()
{
    int rc;
    void *sleep1;
    void *sleep2;
    void *hndl;
    endvars;

    sleep1 = call msleep (&rc, 1000);
    sleep2 = call msleep (&rc, 2000);

    while (1) {
        wait (&hndl, 0);
        assert (rc == 0);
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
mill stopwatch.mc
gcc -o stopwatch stopwatch.c mill.c stdmill.c -luv
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

Mill is more than a coroutine generator though. It tries to tame the
complexity inherent in asynchronous programming by defining strict limits
for coroutine lifetimes. Specifically, no coroutine can exceed the lifetime of
the coroutine that launched it. In other words, when coroutine ends, all the
coroutines it have launched are automatically canceled.

## License

Mill is licensed under MIT/X11 license.
