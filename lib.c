
void musleep(unsigned int microseconds) {
    if (!ring)
        init();
    if(microseconds <= 0)
        return;
    uint64_t ms = microseconds / 1000;
    if(microseconds % 1000 != 0)
        ++ms;
    dotrace("goes to sleep");
    struct cr *current = ring_rm();
    current->expiry = now() + ms;

    struct cr **it = &timers;
    while(*it && (*it)->expiry <= current->expiry)
        it = &((*it)->next);
    current->next = *it;
    *it = current;
    if(setjmp(current->ctx))
        return;
    schedule();    
}

void msleep(unsigned int seconds) {
    musleep(seconds * 1000000);
}

static void after_(int ms, chan ch) {
    musleep(ms * 1000);
    chan_val val;
    val.ptr = NULL;
    chan_send(ch, val);
    chan_close(ch);
}

chan after(int ms) {
    chan ch = chan_init();
    go(after_(ms, ch));
    return ch;
}

