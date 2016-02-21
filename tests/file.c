/*

  Copyright (c) 2016 Paulo Faria

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "../libmill.h"

int main() {
    mfile f1 = mfopen("/tmp/file1", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR |
        S_IWUSR | S_IRGRP | S_IROTH);
    assert(f1);

    assert(mfeof(f1));
    assert(errno == 0);

    mfwrite(f1, "ABC", 3, -1);
    assert(errno == 0);

    mfflush(f1, -1);
    assert(errno == 0);

    assert(mfeof(f1));
    assert(errno == 0);

    assert(mftell(f1) == 3);
    assert(errno == 0);

    mfseek(f1, 0);
    assert(errno == 0);

    assert(!mfeof(f1));
    assert(errno == 0);

    char buf[3];
    size_t sz = mfread(f1, buf, sizeof(buf), -1);
    assert(errno == 0);
    assert(sz == 3 && buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');

    assert(mfeof(f1));
    assert(errno == 0);

    mfclose(f1);

    return 0;
}
