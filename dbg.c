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

static int tracing = 0;

void trace(void) {
    tracing = 1;
}

static void cr_function(struct cr *stack, char *buf, size_t len) { 
    if(!len)
        return;
    const char *it = stack->desc;
    while(len > 1 && (isalnum(*it) || *it == '_')) {
        *buf = *it;
        ++it,++buf,--len;
    }
    *buf = 0;
}

static void dotrace(const char *text) {
    if (!tracing)
        return;
    char func[64];
    cr_function(ring_current(), func, sizeof(func));
    fprintf(stderr, "===> %lu: %s - %s\n", ring_current()->id, func, text);
}

static void goredump_dummy(void) {
}

goredump_fn_ goredump_(const char *file, int line) {
    struct cr *stack;
    char func[256];

    fprintf(stderr, "+--------------------------------------------------------"
        "-----------------------\n");
    fprintf(stderr, "| GOREDUMP (%s:%d)\n", file, line); // TODO: add time
    stack = ring_current();
    if(stack) {
        cr_function(stack, func, sizeof(func));
        fprintf(stderr, "|    %lu: %s - running\n", stack->id, func);
        stack = stack->next;
        if(stack != ring_current()) {
		    while(1) {
                cr_function(stack, func, sizeof(func));
		        fprintf(stderr, "|    %lu: %s - ready\n", stack->id, func);
		        stack = stack->next;
		        if(stack == ring_current())
		            break;
		    }
        }
    }
    if(timers) {
        uint64_t nw = now();
        stack = timers;
        while(stack) {
            unsigned long rmnd = nw > stack->expiry ?
                0 : stack->expiry - nw;
            cr_function(stack, func, sizeof(func));
            fprintf(stderr, "|    %lu: %s - sleeping (%lu ms remaining)\n",
                stack->id, func, rmnd);
            stack = stack->next;
        }
    }
    fprintf(stderr, "+--------------------------------------------------------"
        "-----------------------\n");
    return goredump_dummy;
}
