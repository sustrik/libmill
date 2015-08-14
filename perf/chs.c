/*

  Copyright (c) 2015 Martin Sustrik

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

int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("usage: chs <millions-of-messages>\n");
        return 1;
    }
    long count = atol(argv[1]) * 1000000;

    chan ch = chmake(char, count);

    int64_t start = now();

    long i;
    for(i = 0; i != count; ++i)
        chs(ch, char, 0);

    int64_t stop = now();
    long duration = (long)(stop - start);
    long ns = duration * 1000000 / count;

    printf("sent %ldM messages in %f seconds\n",
        (long)(count / 1000000), ((float)duration) / 1000);
    printf("duration of sending a single message: %ld ns\n", ns);
    printf("message sent per second: %fM\n",
        (float)(1000000000 / ns) / 1000000);

    return 0;
}

