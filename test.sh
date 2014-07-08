#!/bin/sh
./mill --system stdmill.mh
./mill --system stdmill.mc
./mill test.mc
gcc -o test mill.c stdmill.c test.c -luv -g -O0
./test
