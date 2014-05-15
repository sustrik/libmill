#
# Copyright (c) 2013 Luca Barbato
# Copyright (c) 2014 Martin Sustrik  All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom
# the Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

################################################################################
#  Start the configuration phase.                                              #
################################################################################

AC_PREREQ([2.53])

AC_INIT([mill], [0.1])
AC_CONFIG_SRCDIR([mill.h])
AM_INIT_AUTOMAKE([1.6 foreign subdir-objects tar-ustar dist-zip])
m4_ifdef([AM_SILENT_RULES], [
    AM_SILENT_RULES([yes])
])

AC_CANONICAL_HOST

################################################################################
#  Check the compilers.                                                         #
################################################################################

AC_PROG_CC_C99
AM_PROG_CC_C_O

################################################################################
#  If --enable-debug is specified set appropriate compiler options.            #
################################################################################

AC_ARG_ENABLE([debug], [AS_HELP_STRING([--enable-debug],
    [Enable debugging information [default=no]])])

if test "x$enable_debug" = "xyes"; then
    if test "x$GCC" = "xyes" -o \
            "x$ac_cv_icc_compiler" = "xyes" -o \
            "x$ac_cv_clang_compiler" = "xyes"; then
        CFLAGS="-g -O0 "
    elif test "x$ac_cv_sun_studio_compiler" = "xyes"; then
        CFLAGS="-g0 "
    elif test "x$ac_cv_hpacc_compiler" = "xyes"; then
        CFLAGS="-g +O0 "
    fi
    ORIG_CFLAGS="${CFLAGS:-none}"
    if test "x$ORIG_CFLAGS" != "xnone"; then
        CFLAGS="${CFLAGS} ${ORIG_CFLAGS}"
    fi
    AC_DEFINE([NN_DEBUG])
fi

################################################################################
#  Feature checks.                                                             #
################################################################################

AX_PTHREAD([
  LIBS="$PTHREAD_LIBS $LIBS"
  CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
  CC="$PTHREAD_CC"
])

AC_SEARCH_LIBS([uv_tcp_init], [uv], [], [
  AC_MSG_ERROR([Unable to find libuv library.])
])

LT_INIT

DOLT

################################################################################
#  Finish the configuration phase.                                             #
################################################################################

AC_CONFIG_MACRO_DIR([m4])

AC_OUTPUT([
Makefile
libmill.pc
])

