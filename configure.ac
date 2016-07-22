#
# Copyright (c) 2013 Luca Barbato
# Copyright (c) 2015 Martin Sustrik  All rights reserved.
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

AC_INIT([libmill], [m4_esyscmd([./package_version.sh])],
   [libmill@freelists.org], [libmill], [http://libmill.org/])
AC_CONFIG_SRCDIR([libmill.pc.in])
AM_INIT_AUTOMAKE([1.6 foreign subdir-objects tar-ustar])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

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
#  Check the compilers.                                                        #
################################################################################

AC_PROG_CC_C99
AM_PROG_CC_C_O

################################################################################
#  --enable-ssl                                                                #
################################################################################

AC_ARG_ENABLE([ssl], [AS_HELP_STRING([--enable-ssl],
    [Enable SSL sockets [default=no]])])

if test "x$enable_ssl" = "xyes"; then
    AC_DEFINE(HAVE_SSL)
    AC_CHECK_LIB([ssl], [SSL_ctrl], [],
        AC_MSG_ERROR([libssl not found; install OpenSSL]))
    AC_CHECK_LIB([crypto], [BIO_read], [],
        AC_MSG_ERROR([libcrypto not found; install OpenSSL]))
fi

################################################################################
#  --enable-debug                                                              #
################################################################################

AC_ARG_ENABLE([debug], [AS_HELP_STRING([--enable-debug],
    [Enable debugging information [default=no]])])

if test "x$enable_debug" = "xyes"; then
    # Override original optimisation level - last option specified wins.
    CFLAGS="$CFLAGS -g -O0"
fi

################################################################################
#  --enable-valgrind                                                           #
################################################################################

AC_ARG_ENABLE([valgrind], [AS_HELP_STRING([--enable-valgrind],
    [Provide information about coroutine stacks to valgrind [default=no]])])

if test "x$enable_valgrind" = "xyes"; then
    AC_DEFINE(MILL_VALGRIND)
fi

################################################################################
#  Feature checks.                                                             #
################################################################################

AC_CHECK_FUNC([posix_memalign], [AC_DEFINE([HAVE_POSIX_MEMALIGN])])
AC_CHECK_FUNC([mprotect], [AC_DEFINE([HAVE_MPROTECT])])
AC_CHECK_LIB([rt], [clock_gettime])
AC_CHECK_FUNCS([clock_gettime])
AC_CHECK_LIB([socket], [socket])
AC_CHECK_FUNCS([epoll_create], [] ,[AC_DEFINE([MILL_NO_EPOLL])])
AC_CHECK_FUNCS([kqueue], [] ,[AC_DEFINE([MILL_NO_KQUEUE])])

################################################################################
#  Libtool                                                                     #
################################################################################

LT_INIT

################################################################################
#  Finish the configuration phase.                                             #
################################################################################

AC_CONFIG_MACRO_DIR([m4])

AC_OUTPUT([Makefile libmill.pc])
cp confdefs.h config.h

