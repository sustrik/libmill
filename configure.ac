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
