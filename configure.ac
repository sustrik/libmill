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

AC_INIT([stdmill], [m4_esyscmd([./package_version.sh])],
   [millc@freelists.org], [Mill], [http://millc.org/])
AC_CONFIG_SRCDIR([stdmill.h])
AM_INIT_AUTOMAKE([1.6 foreign subdir-objects tar-ustar dist-zip])
m4_ifdef([AM_SILENT_RULES], [
    AM_SILENT_RULES([yes])
])

AC_CANONICAL_HOST

################################################################################
#  Retrieve the versions.                                                      #
################################################################################

AC_PROG_SED
AC_PROG_AWK

MILL_ABI_VERSION=m4_esyscmd([./abi_version.sh])
MILL_PACKAGE_VERSION=m4_esyscmd([./package_version.sh])
MILL_LIBTOOL_VERSION=m4_esyscmd([./abi_version.sh -libtool])

AC_SUBST(MILL_ABI_VERSION)
AC_SUBST(MILL_PACKAGE_VERSION)
AC_SUBST(MILL_LIBTOOL_VERSION)

AC_MSG_NOTICE([Mill package version: $MILL_PACKAGE_VERSION])
AC_MSG_NOTICE([Mill ABI version: $MILL_ABI_VERSION])

################################################################################
#  Check the compilers.                                                         #
################################################################################

AC_PROG_CC_C99
AM_PROG_CC_C_O

################################################################################
#  If --enable-debug is specified set appropriate compiler options.            #
################################################################################

AC_ARG_ENABLE([gen], [AS_HELP_STRING([--enable-gen],
    [Enable code generation (for Mill developers) [default=no]])])
AM_CONDITIONAL([GEN], [test x"$enable_gen" = "xyes"])

AC_ARG_ENABLE([debug], [AS_HELP_STRING([--enable-debug],
    [Enable debugging information [default=no]])])

if test "x$enable_debug" = "xyes"; then
    CFLAGS="-g -O0 "
    ORIG_CFLAGS="${CFLAGS:-none}"
    if test "x$ORIG_CFLAGS" != "xnone"; then
        CFLAGS="${CFLAGS} ${ORIG_CFLAGS}"
    fi
    AC_DEFINE([MILL_DEBUG])
fi

################################################################################
#  Feature checks.                                                             #
################################################################################

AC_PATH_PROG([RUBY], [ruby])
AS_IF([test x"$RUBY" = "x"],[
    AC_MSG_ERROR([Mill requires Ruby to operate, please install it.])
])

AC_CHECK_LIB([uv], [uv_close], [], [
    AC_MSG_ERROR([Mill requires libuv to operate, please install it from https://github.com/joyent/libuv.])
])

# Additional dependencies for the code generation phase.
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

LT_INIT

DOLT

################################################################################
#  Finish the configuration phase.                                             #
################################################################################

AC_CONFIG_MACRO_DIR([m4])

AC_OUTPUT([
Makefile
libstdmill.pc
])
cp confdefs.h config.h

