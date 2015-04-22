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
