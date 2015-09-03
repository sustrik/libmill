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

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include "../libmill.h"

static coroutine void whisper(chan left, chan right) {
    int val = chr(right, int);
    chs(left, int, val + 1);
}

int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("usage: whispers <number-of-whispers>\n");
        return 1;
    }

    long count = atol(argv[1]);
    int64_t start = now();

    chan leftmost = chmake(int, 0);
    chan left = leftmost, right = leftmost;
    long i;
    for (i = 0; i < count; ++i) {
        right = chmake(int, 0);
        go(whisper(left, right));
        left = right;
    }

    chs(right, int, 1);
    int res = chr(leftmost, int);
    assert(res == count + 1);

    int64_t stop = now();
    long duration = (long)(stop - start);

    printf("took %f seconds\n", (float)duration / 1000);

    return 0;
}
