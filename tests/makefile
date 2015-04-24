
all: test_go

.PHONY: test_go 
test_go: go
	./go
go: go.c ../mill.c ../mill.h
	cc -o go go.c ../mill.c -g -O0

.PHONY: clean
clean:
	rm -f go sleep chan
