AC_PREREQ([2.53])
AC_INIT([mill], [0.1])

AC_PATH_PROG([RUBY], [ruby])
AS_IF([test x"$RUBY" = "x"],[
    AC_MSG_ERROR([Mill requires ruby to operate, please install it])
])

AC_PATH_PROG([RIBOSOME], [ribosome])
AS_IF([test x"$RIBOSOME" = "x"],[
    AC_MSG_ERROR([Mill requires ribosome to operate, please install it])
])

AC_CHECK_LIB([uv], [uv_close], [], [
    AC_MSG_ERROR([Mill requires libuv to operate, please install it])
])

AC_PROG_CC
AC_PROG_RANLIB

AM_INIT_AUTOMAKE([foreign])
AC_OUTPUT([Makefile])
