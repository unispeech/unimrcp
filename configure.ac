#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([unimrcp], [0.0.1])

AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build/acmacros])
AC_PREFIX_DEFAULT(/usr/local/unimrcp)

AM_INIT_AUTOMAKE(foreign)

#Set default language
AC_LANG_C

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

#skip detection of Fortran
m4_undefine([AC_PROG_F77])
m4_defun([AC_PROG_F77],[])
AC_PROG_LIBTOOL


UNIMRCP_CHECK_APR
UNIMRCP_CHECK_APU

AC_SUBST(ac_aux_dir)
AC_SUBST(ac_macro_dir)


AC_ARG_ENABLE(maintainer-mode,
[AC_HELP_STRING([--enable-maintainer-mode  ],[turn on debugging and compile time warnings])],
[enable_maintainer_mode="$enable_maintainer_mode"],[enable_maintainer_mode="no"])

if test "${enable_maintainer_mode}" != "no"; then
    CFLAGS="$CFLAGS -g"
    if test "x${ax_cv_c_compiler_vendor}"  =  "xgnu" ; then
      CFLAGS="$CFLAGS -Wall -Werror"
    fi
fi


AC_CONFIG_FILES([
  Makefile
  libs/Makefile
  libs/apr-toolkit/Makefile
])

AC_OUTPUT
