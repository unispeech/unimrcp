dnl
dnl Autoconf configuration file for the UniMRCP project.
dnl
dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)

AC_INIT([unimrcp],[1.1.0])

AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build/acmacros])

dnl Set ac_macro_dir variable manually for autoconf 2.61 and above.
ac_macro_dir="build/acmacros"

AC_SUBST(ac_aux_dir)
AC_SUBST(ac_macro_dir)

AC_PREFIX_DEFAULT(/usr/local/unimrcp)
dnl Since AC_PREFIX_DEFAULT is expanded late, explicitly set the prefix.
if test "x$prefix" = "xNONE" ; then
    prefix="/usr/local/unimrcp"
fi

dnl Set plugin, conf, log and data dir variables.
plugindir="${prefix}/plugin"
confdir="${prefix}/conf"
logdir="${prefix}/log"
datadir="${prefix}/data"

AC_SUBST(plugindir)
AC_SUBST(confdir)
AC_SUBST(logdir)
AC_SUBST(datadir)

dnl Generate ./config.nice to reuse ./configure command-line.
APR_CONFIG_NICE(config.nice)

dnl Include m4 macros for libtool 2.
sinclude(build/acmacros/libtool.m4)
sinclude(build/acmacros/ltoptions.m4)
sinclude(build/acmacros/ltsugar.m4)
sinclude(build/acmacros/ltversion.m4)
sinclude(build/acmacros/lt~obsolete.m4)

AM_INIT_AUTOMAKE([no-define nostdinc foreign])

dnl Enable silent build rules available since automake 1.11.
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Set default language.
AC_LANG_C

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

dnl Skip detection of Fortran.
m4_undefine([AC_PROG_F77])
m4_defun([AC_PROG_F77],[])
AC_PROG_LIBTOOL

dnl Do not use autoconf generated compiler DEFS.
rm confdefs.h
touch confdefs.h

dnl Check for C compiler vendor.
AX_COMPILER_VENDOR

dnl Search for pkg-config.
AC_PATH_PROG(PKG_CONFIG, pkg-config)

dnl Get version information.
get_version="build/get-version.sh"
version_hdr="build/uni_version.h"
plugin_version_hdr="libs/mrcp-engine/include/mrcp_engine_plugin.h"
UNI_DOTTED_VERSION="`$get_version all $version_hdr UNI`"
UNI_LT_VERSION="-version-info `$get_version libtool $version_hdr UNI`"
PLUGIN_LT_VERSION="-version-info `$get_version libtool $plugin_version_hdr PLUGIN`"

AC_SUBST(UNI_DOTTED_VERSION)
AC_SUBST(UNI_LT_VERSION)
AC_SUBST(PLUGIN_LT_VERSION)

echo "UniMRCP Version: ${UNI_DOTTED_VERSION}"

dnl Check for the APR and APR-util libraries.
UNIMRCP_CHECK_APR
dnl Check for the Sofia-SIP library.
UNIMRCP_CHECK_SOFIA

dnl Enable inter-library dependencies.
AC_ARG_ENABLE(interlib-deps,
    [AC_HELP_STRING([--disable-interlib-deps  ],[disable inter-library dependencies (might break builds)])],
    [enable_interlib_deps="$enableval"],
    [enable_interlib_deps="yes"])

AC_MSG_NOTICE([enable inter-library dependencies: $enable_interlib_deps])
if test "${enable_interlib_deps}" == "yes"; then
    link_all_deplibs=yes
    link_all_deplibs_CXX=yes
else
    link_all_deplibs=no
    link_all_deplibs_CXX=no
fi

dnl Enable maintainer mode.
AC_ARG_ENABLE(maintainer-mode,
    [AC_HELP_STRING([--enable-maintainer-mode  ],[turn on debugging and compile time warnings])],
    [enable_maintainer_mode="$enableval"],
    [enable_maintainer_mode="no"])

AC_MSG_NOTICE([enable maintainer mode: $enable_maintainer_mode])
if test "${enable_maintainer_mode}" != "no"; then
    APR_ADDTO(CFLAGS,-g)
    if test "x${ax_cv_c_compiler_vendor}"  =  "xgnu" ; then
        APR_ADDTO(CFLAGS,-Wall -Werror)
    fi
fi

dnl Enable test suites.
AC_ARG_ENABLE(test-suites,
    [AC_HELP_STRING([--enable-test-suites  ],[build test suites])],
    [enable_test_suites="$enableval"],
    [enable_test_suites="no"])

AM_CONDITIONAL([TEST_SUITES],[test "${enable_test_suites}" != "no"])

dnl Demo synthesizer plugin.
UNI_PLUGIN_ENABLED(demosynth)

AM_CONDITIONAL([DEMOSYNTH_PLUGIN],[test "${enable_demosynth_plugin}" = "yes"])

dnl Demo recognizer plugin.
UNI_PLUGIN_ENABLED(demorecog)

AM_CONDITIONAL([DEMORECOG_PLUGIN],[test "${enable_demorecog_plugin}" = "yes"])

dnl Demo verifier plugin.
UNI_PLUGIN_ENABLED(demoverifier)

AM_CONDITIONAL([DEMOVERIFIER_PLUGIN],[test "${enable_demoverifier_plugin}" = "yes"])

dnl Recorder plugin.
UNI_PLUGIN_ENABLED(recorder)

AM_CONDITIONAL([RECORDER_PLUGIN],[test "${enable_recorder_plugin}" = "yes"])

AM_CONDITIONAL(ISMAC, [test `uname -s` = Darwin])

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
    libs/uni-rtsp/Makefile
    modules/Makefile
    modules/mrcp-sofiasip/Makefile
    modules/mrcp-unirtsp/Makefile
    plugins/Makefile
    plugins/mrcp-recorder/Makefile
    plugins/demo-synth/Makefile
    plugins/demo-recog/Makefile
    plugins/demo-verifier/Makefile
    platforms/Makefile
    platforms/libunimrcp-server/Makefile
    platforms/libunimrcp-client/Makefile
    platforms/unimrcp-server/Makefile
    platforms/unimrcp-client/Makefile
    platforms/libasr-client/Makefile
    platforms/asr-client/Makefile
    platforms/umc/Makefile
    tests/Makefile
    tests/apttest/Makefile
    tests/mpftest/Makefile
    tests/mrcptest/Makefile
    tests/rtsptest/Makefile
    tests/strtablegen/Makefile
    build/Makefile
    build/pkgconfig/Makefile
    build/pkgconfig/unimrcpclient.pc
    build/pkgconfig/unimrcpserver.pc
    build/pkgconfig/unimrcpplugin.pc
    build/svnrev/Makefile
    conf/Makefile
    data/Makefile
    docs/doxygen.conf
])

AC_OUTPUT

echo
echo '****************************** REPORT ******************************'
echo
echo UniMRCP version............... : $UNI_DOTTED_VERSION
echo
echo APR version................... : $apr_version
echo APR-util version.............. : $apu_version
echo Sofia-SIP version............. : $sofia_version
echo
echo Compiler...................... : $CC
echo Compiler flags................ : $CFLAGS
echo Preprocessor definitions...... : $CPPFLAGS
echo Linker flags.................. : $LDFLAGS
echo
echo Demo synthesizer plugin....... : $enable_demosynth_plugin
echo Demo recognizer plugin........ : $enable_demorecog_plugin
echo Demo verifier plugin.......... : $enable_demoverifier_plugin
echo Recorder plugin............... : $enable_recorder_plugin
echo
echo Installation directory........ : $prefix
echo
echo '********************************************************************'
