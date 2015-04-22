
struct chan {
    struct cr *sender;
    struct cr *receiver;
    chan_val val;
    chan_val *dest;
    int idx;
    int *idxdest;
};

typedef struct chan* chan;

chan chan_init(void) {
    struct chan *ch = (struct chan*)malloc(sizeof(struct chan));
    assert(ch);
    ch->sender = NULL;
    ch->receiver = NULL;
    ch->val.ptr = NULL;
    ch->dest = NULL;
    ch->idx = -1;
    ch->idxdest = NULL;
    return ch;
}

void chan_send(chan ch, chan_val val) {
    assert(!ch->sender);

    /* If there's a receiver already waiting, we can just unblock it. */
    if(ch->receiver) {
        if(ch->dest)
            *(ch->dest) = val;
        if(ch->idxdest)
            *(ch->idxdest) = ch->idx;
        ring_push(ch->receiver);
        ch->receiver = NULL;
        ch->dest = NULL;
        return;
    }

    /* Otherwise we are going to yield till the receiver arrives. */
    ch->sender = ring_current();
    ch->val = val;
    if(setjmp(ring_current()->ctx)) {
        dotrace("resuming");
        return;
    }
    dotrace("blocks on chan_send");
    ring_rm();
    schedule();
}

chan_val chan_recv(chan ch) {
    chan_val val;

    assert(!ch->receiver);

    /* If there's a sender already waiting, we can just unblock it. */
    if(ch->sender) {
        val = ch->val;
        ring_push(ch->sender);
        ch->sender = NULL;
        ch->val.ptr = NULL;
        return val;
    }

    /* Otherwise we are going to yield till the sender arrives. */
    ch->receiver = ring_current();
    ch->dest = &val;
    if(setjmp(ring_current()->ctx)) {
        dotrace("resuming");
        return val;
    }
    dotrace("blocks on chan_recv");
    ring_rm();
    schedule();
}	

void chan_close(chan ch) {
    // TODO
    free(ch);
}

int chan_tryselectv(chan_val *val, chan *chans, int nchans) {
    /* TODO: For fairness sake we should choose a random number up to 'count'
       and start the array traversal at that index. */
    int i;
    for(i = 0; i != nchans; ++i) {
        chan ch = chans[i];
        assert(!ch->receiver);
		if(ch->sender) {
            if(val)
		        *val = ch->val;
		    ring_push(ch->sender);
		    ch->sender = NULL;
		    ch->val.ptr = NULL;
		    return i;
		}
    }
    return -1;
}

int chan_selectv(chan_val *val, chan *chans, int nchans) {

    /* Try to select a channel in a non-blocking way. */
    int res = chan_tryselectv(val, chans, nchans);
    if(res >= 0)
        return res;

    /* None of the channels is ready. Mark them as such. */
    int i;
    for(i = 0; i != nchans; ++i) {
        chan ch = chans[i];
	    ch->receiver = ring_current();
		ch->dest = val;
        ch->idx = i;
        ch->idxdest = &res;
    }

    /* Yield till one of the channels becomes ready. */
	if(!setjmp(ring_current()->ctx)) {
		dotrace("blocks on chan_select");
		ring_rm();
		schedule();
    }

    /* One of the channels became available. */
    for(i = 0; i != nchans; ++i) {
         chan ch = chans[i];
         ch->receiver = NULL;
         ch->dest = NULL;
         ch->idx = -1;
         ch->idxdest = NULL;
    }

    /* This coroutine have got to the front of the queue unfairly.
       Let's put it back where it belongs. */
    /* TODO: We should not mess with the coroutines position in the ring
       at all .*/
    yield();

    return res;
}

int chan_select_(int block, chan_val *val, ...) {
    int count = 0;
    va_list v;
    va_start(v, val);
    while(1) {
        chan ch = va_arg(v, chan);
        if(!ch)
            break;
        ++count;
    }
    va_end(v);
    assert(count > 0);
    chan chans[count];
    int i;
    va_start(v, val);
    for(i = 0; i != count; ++i)
        chans[i] = va_arg(v, chan);
    va_end(v);
    if(block)
        return chan_selectv(val, chans, count);
    return chan_tryselectv(val, chans, count);
}

