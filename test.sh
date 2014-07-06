#!/bin/sh
./mill --system stdmill.mill
./mill test.mill
gcc -o test mill.c stdmill.c test.c -luv -g -O0
./test
