./mill sleep.mill
gcc -o sleep sleep.c mill.c -luv -g -O0
./sleep
./mill tcpsocket.mill
gcc -o tcpsocket tcpsocket.c mill.c -luv -g -O0
./tcpsocket
./mill getaddressinfo.mill
gcc -o getaddressinfo getaddressinfo.c mill.c -luv -g -O0
./getaddressinfo

./mill hello.mill
gcc -o hello hello.c mill.c -luv -g -O0
./mill world.mill
gcc -o world world.c mill.c -luv -g -O0
