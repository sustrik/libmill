/*
    Copyright (c) 2014 Martin Sustrik  All rights reserved.

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

#ifndef MILL_H_INCLUDED
#define MILL_H_INCLUDED

#include <assert.h>
#include <stdlib.h>

typedef void* event;

struct ____base;

typedef void (*____handler_fn) (struct ____base *self, event ev);

struct ____base {
    ____handler_fn handler; 
    int state;
};

void ____base_init (struct ____base *self, ____handler_fn handler); 

#define ____getevent(statearg, eventarg)\
    self->____base.state = (statearg);\
    return;\
    state_##statearg:\
    (eventarg) = ev;

#define ____return(resarg)\
    self->____result = (resarg);\
    return;

#endif

