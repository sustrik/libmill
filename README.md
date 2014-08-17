# Mill

Mill is a preprocessor that adds coroutine support to C language.

The result of preprocessing are standard C source and header files that can be
added directly to your C project.

The project also contains library of elementary coroutines (timers,
network I/O et c.) that can be used to build more complex functionality.

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
    go msleep (1000);
    go msleep (2000);

    while (1) {
        select {
        case msleep:
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

For more info check the Mill website: http://millc.org

## License

Mill is licensed under MIT/X11 license.

