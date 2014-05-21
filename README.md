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
#include <alarm.h>

coroutine quux ()
{
    call alarm (1000);
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

### Introduction

Coroutines are a way to implement cooperative multitasking in a rather simple
and developer-friendly manner.

If you are not familiar with coroutines, check the wikipedia article here:

https://en.wikipedia.org/wiki/Coroutine

A nice explanation of how to implement coroutines in C can be found here:

http://www.chiark.greenend.org.uk/~sgtatham/coroutines.html

### Mill

To use coroutines, it is best to choose a language that has direct support for
the concept, such as Go. However, sometimes -- mostly when writing system-level
code -- C is the only option. After all, C is the lingua franca of programming
languages and every single language as well as every single OS integrates well
with C.

Writing coroutines by hand (see the link above) is an option, however, once
you move beyond the very basic functionality the all the required boilerplate
code is both annoying and confusing.

Mill tries to solve this problem by defining a slightly augmented version of
C with support for coroutines which compiles (or "pre-processes", if you will)
into straight C.

### Files

To distinguish mill files from straight C, they should use .mill extension.
Each mill file is compiled into a C header file and a straight C file.

The compiler (or the pre-processor) is called "mill":

```
$ mill test.mill
$ ls
test.c test.h test.mill
```

The generated header file can be included by other mill files or, for what it's
worth, by other C programs to be able to invoke coroutines defined therein:

```
#include "test.h"

int main ()
{
    test ();
    return 0;
}
```

### Defining coroutines

To define a new coroutine use "coroutine" keyword. The coroutine definition
is similar to a standard C function definition. One thing to note is that mill
coroutines don't have a return type. From C point of view they can be though
of as void functions:

```
#include <stdio.h>

coroutine hello ()
{
    printf ("Hello, world!");
}
```

## License

Mill is licensed under MIT/X11 license.
