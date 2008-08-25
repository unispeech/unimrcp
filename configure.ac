#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([unimrcp], [0.0.1])

AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build/acmacros])
AC_PREFIX_DEFAULT(/usr/local/unimrcp)
plugindir='${prefix}/plugin'

plugindir='${prefix}/plugin'

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

UNIMRCP_CHECK_SOFIA

AC_SUBST(ac_aux_dir)
AC_SUBST(ac_macro_dir)
AC_SUBST(plugindir)

AC_DEFUN([AX_COMPILER_VENDOR],
[
AC_CACHE_CHECK([for _AC_LANG compiler vendor], ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor,
 [ax_cv_[]_AC_LANG_ABBREV[]_compiler_vendor=unknown
  # note: don't check for gcc first since some other compilers define __GNUC__
  for ventest in intel:__ICC,__ECC,__INTEL_COMPILER ibm:__xlc__,__xlC__,__IBMC__,__IBMCPP__ gnu:__GNUC__ sun:__SUNPRO_C,__SUNPRO_CC hp:__HP_cc,__HP_aCC dec:__DECC,__DECCXX,__DECC_VER,__DECCXX_VER borland:__BORLANDC__,__TURBOC__ comeau:__COMO__ cray:_CRAYC kai:__KCC lcc:__LCC__ metrowerks:__MWERKS__ sgi:__sgi,sgi microsoft:_MSC_VER watcom:__WATCOMC__ portland:__PGI; do
    vencpp="defined("`echo $ventest | cut -d: -f2 | sed 's/,/) || defined(/g'`")"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[
#if !($vencpp)
      thisisanerror;
#endif
])], [ax_cv_]_AC_LANG_ABBREV[_compiler_vendor=`echo $ventest | cut -d: -f1`; break])
  done
 ])
])

AX_COMPILER_VENDOR


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
  libs/mpf/Makefile
  libs/mrcp/Makefile
  libs/mrcp-signaling/Makefile
  libs/mrcpv2-transport/Makefile
  libs/mrcp-engine/Makefile
  libs/mrcp-server/Makefile
  libs/mrcp-client/Makefile
  modules/Makefile
  modules/mrcp-sofiasip/Makefile
  plugins/Makefile
  plugins/demo-synth/Makefile
  plugins/demo-recog/Makefile
  platforms/Makefile
  platforms/libunimrcp-server/Makefile
  platforms/libunimrcp-client/Makefile
  platforms/unimrcp-server/Makefile
  platforms/unimrcp-client/Makefile
  tests/Makefile
  tests/apttest/Makefile
  tests/mpftest/Makefile
  tests/mrcptest/Makefile
  tests/strtablegen/Makefile
])

AC_OUTPUT
