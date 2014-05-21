Mill
====

Mill is a C preprocessor to create coroutines from what looks more or less
like an ordinary C code.

WARNING: This project is under development. Do not use!

## Prerequisites

* Install Ruby
* Install libuv from here: https://github.com/joyent/libuv

The former is needed at compilation time, the latter at both compilation time
and runtime.

## Example

```
#include <stdio.h>
#include <msleep.h>

coroutine quux ()
{
    call msleep (1000);
    printf ("Waiting for timeout to expire...\n");
    wait;
    printf ("Done!\n");
}

int main ()
{
    quux ();
    return 0;
}
```

Note that the printf will be executed in parallel with the alarm coroutine!

## Usage

```
./mill example.mill
gcc -o example example.c mill.c -luv
./example
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

However, sometimes -- mostly when writing system-level code -- there is no other
option but to use C. After all, C is the lingua franca of programming
languages and every single language as well as every single OS integrates well
with C.

Writing coroutines by hand (see the link above) is an option, but once you move
beyond the very basic functionality all the required boilerplate code is both
annoying to write and confusing to read.

Mill tries to solve this problem by defining a slightly augmented version of
C language, one with direct support for coroutines, which compiles into
straight C.

### Defining coroutines

To define a new coroutine use "coroutine" keyword. The coroutine definition
is similar to a standard C function definition. One difference to note though
is that mill coroutines don't have a return type. From C point of view they can
be though of as void functions:

```
#include <stdio.h>

coroutine hello ()
{
    printf ("Hello, world!");
}
```

Please note that coroutines should not perform any blocking operations.
The nature of cooperative multitasking is such that a single blocked coroutine
can block all the other simultaneously running coroutines. To perform
classic blocking operations such as sleeping or reading from a socket, mill
provides a set of pre-defined coroutines. See below.

### Compilation

To distinguish mill files from straight C, they should use .mill extension.
Each mill file is compiled into a C header file and a straight C file.

The compiler is called "mill":

```
$ ls
hello.mill
$ mill hello.mill
$ ls
hello.c hello.h hello.mill
```

The generated header file can be included by other mill files or, for what it's
worth, by other C programs so that they are able to invoke coroutines defined
therein:

```
#include "hello.h"

int main ()
{
    hello ();
    return 0;
}
```

### Asynchronous execution

If coroutine is invoked as if it was a classic C function (see the example
above) it behaves in a synchronous manner. The call returns only when the
coroutine is fully executed.

To invoke coroutine in an asynchronous manner, use "call" keyword. The keyword
can be used only within a coroutine body. Following example shows how to
use standard "msleep" coroutine that does nothing but waits for specified number
of milliseconds before terminating:

```
#include <msleep.h>

coroutine foo ()
{
    call msleep (1000);
}

int main ()
{
    foo ();
    return 0;
}
```


## License

Mill is licensed under MIT/X11 license.
