Mill
====

Mill is a C preprocessor to create coroutines from what looks like an
ordinary C code.

WARNING: This project is under development. Do not use!

## Prerequisites

* Install Ruby
* Install libuv from here: https://github.com/joyent/libuv

The former is needed at compilation time, the latter at both compilation time
and runtime.

## Example

```
#include <stdio.h>
#include <assert.h>

coroutine quux ()
{
    coroutine wait w;
    event e;
    endvars;

    call wait (&w, 1000);
    printf ("Waiting for timeout to expire...\n");
    getevent e;
    assert (e == &w);
    printf ("Done!\n");
}

int main ()
{
    quux ();
    return 0;
}
```

## Usage

```
./mill example.mill
gcc -o example example.c mill.c -luv
./example
```

Note that the first printf is executed in parallel with the wait coroutine!

## License

Mill is licensed under MIT/X11 license.
