Mill
====

Mill is a C preprocessor to create coroutines from what looks like an
ordinary C code.

WARNING: This project is under development. Do not use!

## Prerequisites

* Install ribosome from here: https://github.com/sustrik/ribosome
* Install libuv from https://github.com/joyent/libuv

The former is needed at compilation time, the latter at both compilation time
and runtime.

## Usage

```
./mill example.mill
gcc -o example example.c mill.c -luv -g -O0
./example
```

## License

Mill is licensed under MIT/X11 license.
