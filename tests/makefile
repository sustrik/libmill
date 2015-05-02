

MILLHDRS = ../mill.h
MILLSRC = ../mill.c
MILLFILES = $(MILLHDRS) $(MILLSRC)
CCFLAGS = -g -O0

all: test_example test_go test_chan test_choose test_sleep test_socket

.PHONY: test_example 
test_example: example
	./example > example.out
example: example.c $(MILLFILES)
	cc -o example example.c $(MILLSRC) $(CCFLAGS)

.PHONY: test_go 
test_go: go
	./go
go: go.c $(MILLFILES)
	cc -o go go.c $(MILLSRC) $(CCFLAGS)

.PHONY: test_chan
test_chan: chan
	./chan
chan: chan.c $(MILLFILES)
	cc -o chan chan.c $(MILLSRC) $(CCFLAGS)

.PHONY: test_choose
test_choose: choose
	./choose
choose: choose.c $(MILLFILES)
	cc -o choose choose.c $(MILLSRC) $(CCFLAGS)

.PHONY: test_sleep
test_sleep: sleep
	./sleep
sleep: sleep.c $(MILLFILES)
	cc -o sleep sleep.c $(MILLSRC) $(CCFLAGS)

.PHONY: test_socket
test_socket: socket
	./socket
socket: socket.c $(MILLFILES)
	cc -o socket socket.c $(MILLSRC) $(CCFLAGS)

.PHONY: clean
clean:
	rm -f example go chan choose sleep socket
