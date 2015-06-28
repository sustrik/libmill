/*

  Copyright (c) 2015 Alex Cornejo

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#include "../libmill.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

static uint64_t now() {
    struct timeval tv;
    int rc = gettimeofday(&tv, NULL);
    assert(rc == 0);
    return ((uint64_t)tv.tv_sec) * 1000 + (((uint64_t)tv.tv_usec) / 1000);
}

static void whisper(chan left, chan right) {
    int val = chr(right, int);
    chs(left, int, val+1);
}

int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("usage: whispers <number-of-whispers>\n");
        return 1;
    }

    long count = atol(argv[1]);
    uint64_t start = now();

    long i;
    chan leftmost = chmake(int, 0);
    chan left = leftmost, right = leftmost;
    for (i = 0; i < count; ++i) {
        right = chmake (int, 0);
        go(whisper(left, right));
        left = right;
    }

    chs(right, int, 1);
    printf("%d\n", chr(leftmost, int));

    uint64_t stop = now();
    long duration = (long)(stop - start);

    printf ("took %f seconds\n", (float)duration / 1000);

    return 0;
}
