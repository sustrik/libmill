
CFLAGS=-g -O0

test: test.o mill.o mill.h

all: test

clean:
	rm test test.o mill.o
