
all: test_go test_chan

.PHONY: test_go 
test_go: go
	./go
go: go.c ../mill.c ../mill.h
	cc -o go go.c ../mill.c -g -O0

.PHONY: test_chan
test_chan: chan
	./chan
chan: chan.c ../mill.c ../mill.h
	cc -o chan chan.c ../mill.c -g -O0

.PHONY: clean
clean:
	rm -f go chan
