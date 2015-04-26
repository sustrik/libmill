
all: test_go test_chan test_chselect

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

.PHONY: test_chselect
test_chselect: chselect
	./chselect
chselect: chselect.c ../mill.c ../mill.h
	cc -o chselect chselect.c ../mill.c -g -O0

.PHONY: clean
clean:
	rm -f go chan chselect
