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

## License

Mill is licensed under MIT/X11 license.
