./mill sleep.mill
gcc -o sleep sleep.c mill.c -luv
./sleep

./mill hello.mill
gcc -o hello hello.c mill.c -luv
./mill world.mill
gcc -o world world.c mill.c -luv
