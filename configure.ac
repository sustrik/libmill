#
#   Copyright (c) 2014 Martin Sustrik  All rights reserved.
#
#   Permission is hereby granted, free of charge, to any person obtaining a copy
#   of this software and associated documentation files (the "Software"),
#   to deal in the Software without restriction, including without limitation
#   the rights to use, copy, modify, merge, publish, distribute, sublicense,
#   and/or sell copies of the Software, and to permit persons to whom
#   the Software is furnished to do so, subject to the following conditions:
#
#   The above copyright notice and this permission notice shall be included
#   in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
#   IN THE SOFTWARE.
#

AC_PREREQ([2.53])
AC_INIT([mill], [0.1])

AC_PATH_PROG([RUBY], [ruby])
AS_IF([test x"$RUBY" = "x"],[
    AC_MSG_ERROR([Mill requires Ruby to operate, please install it.])
])

AC_CHECK_LIB([uv], [uv_close], [], [
    AC_MSG_ERROR([Mill requires libuv to operate, please install it from https://github.com/joyent/libuv.])
])

AC_ARG_ENABLE([gen], [AS_HELP_STRING([--enable-gen],
    [Enable code generation (for Mill developers) [default=no]])])

AS_IF([test x"$enable_gen" = "xyes"], [
    AC_PATH_PROG([RIBOSOME], [ribosome])
    AS_IF([test x"$RIBOSOME" = "x"],[
        AC_MSG_ERROR([Code generation requires ribosome, please install it from https://github.com/sustrik/ribosome.])
    ])
    AC_PATH_PROG([RAGEL], [ragel])
    AS_IF([test x"$RAGEL" = "x"],[
        AC_MSG_ERROR([Code generation requires Ragel, please install it.])
    ])
])
AM_CONDITIONAL([GEN], [test x"$enable_gen" = "xyes"])

AC_PROG_CC
AC_PROG_RANLIB

AM_INIT_AUTOMAKE([foreign])
AC_OUTPUT([Makefile])
