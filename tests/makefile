
all: test_example test_go test_chan test_choose

.PHONY: test_example 
test_example: example
	./example
example: example.c ../mill.c ../mill.h
	cc -o example example.c ../mill.c -g -O0

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

.PHONY: test_choose
test_choose: choose
	./choose
choose: choose.c ../mill.c ../mill.h
	cc -o choose choose.c ../mill.c -g -O0

.PHONY: clean
clean:
	rm -f example go chan choose
