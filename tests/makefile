

MILLHDRS = ../mill.h
MILLSRC = ../mill.c
MILLFILES = $(MILLHDRS) $(MILLSRC)
CCFLAGS = -g -O0
CC = cc

all: test_example \
     test_go \
     test_cls \
     test_chan \
     test_choose \
     test_sleep \
     test_fdwait \
     test_tcp

.PHONY: test_example 
test_example: example
	./example > example.out
example: example.c $(MILLFILES)
	$(CC) -o example example.c $(MILLSRC) $(CCFLAGS)

.PHONY: test_go 
test_go: go
	./go
go: go.c $(MILLFILES)
	$(CC) -o go go.c $(MILLSRC) $(CCFLAGS)

.PHONY: test_cls
test_cls: cls
	./cls
cls: cls.c $(MILLFILES)
	$(CC) -o cls cls.c $(MILLSRC) $(CCFLAGS)

.PHONY: test_chan
test_chan: chan
	./chan
chan: chan.c $(MILLFILES)
	$(CC) -o chan chan.c $(MILLSRC) $(CCFLAGS)

.PHONY: test_choose
test_choose: choose
	./choose
choose: choose.c $(MILLFILES)
	$(CC) -o choose choose.c $(MILLSRC) $(CCFLAGS)

.PHONY: test_sleep
test_sleep: sleep
	./sleep
sleep: sleep.c $(MILLFILES)
	$(CC) -o sleep sleep.c $(MILLSRC) $(CCFLAGS)

.PHONY: test_fdwait
test_fdwait: fdwait
	./fdwait
fdwait: fdwait.c $(MILLFILES)
	$(CC) -o fdwait fdwait.c $(MILLSRC) $(CCFLAGS)

.PHONY: test_tcp
test_tcp: tcp
	./tcp
tcp: tcp.c $(MILLFILES)
	$(CC) -o tcp tcp.c $(MILLSRC) ../tcp.c $(CCFLAGS)

.PHONY: clean
clean:
	rm -f example go cls chan choose sleep fdwait tcp

