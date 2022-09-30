dnl
dnl UNIMRCP_CHECK_OPENCORE_AMR
dnl
dnl This macro attempts to find the OpenCORE AMR library and
dnl set corresponding variables on exit.
dnl
AC_DEFUN([UNIMRCP_CHECK_OPENCORE_AMR],
[
    AC_MSG_NOTICE([OpenCORE AMR library configuration])

    AC_MSG_CHECKING([for OpenCORE AMR])
    AC_ARG_WITH(opencore-amr,
                [  --with-opencore-amr=PATH   prefix for installed OpenCORE AMR,
                          or the full path to OpenCORE AMR pkg-config],
                [opencore_amr_path=$withval],
                [opencore_amr_path="/usr/local/opencore"]
                )

    found_opencore_amr="no"

    if test -n "$PKG_CONFIG"; then
        dnl Check for installed OpenCORE AMR
        for dir in $opencore_amr_path ; do
            opencore_amr_config_path=$dir/lib/pkgconfig/opencore-amrwb.pc
            if test -f "$opencore_amr_config_path" && $PKG_CONFIG $opencore_amr_config_path > /dev/null 2>&1; then
                found_opencore_amr="yes"
                break
            fi
        done

        dnl Check for full path to OpenCORE AMR pkg-config file
        if test "$found_opencore_amr" != "yes" && test -f "$opencore_amr_path" && $PKG_CONFIG $opencore_amr_path > /dev/null 2>&1 ; then
            found_opencore_amr="yes"
            opencore_amr_config_path=$opencore_amr_path
        fi

        if test "$found_opencore_amr" = "yes" ; then
            UNIMRCP_OPENCORE_AMR_INCLUDES="`$PKG_CONFIG --cflags $opencore_amr_config_path`"
            UNIMRCP_OPENCORE_AMR_LIBS="`$PKG_CONFIG --libs $opencore_amr_config_path`"
            opencore_amr_version="`$PKG_CONFIG --modversion $opencore_amr_config_path`"
        fi
    fi

    if test $found_opencore_amr != "yes" ; then
        AC_MSG_ERROR(Cannot find OpenCORE AMR - looked in $opencore_amr_path)
    else
        AC_MSG_RESULT([$found_opencore_amr])
        AC_MSG_RESULT([$opencore_amr_version])

case "$host" in
    *darwin*)
        UNIMRCP_OPENCORE_AMR_LIBS="$UNIMRCP_OPENCORE_AMR_LIBS -framework CoreFoundation -framework SystemConfiguration"
        ;;
esac

        AC_SUBST(UNIMRCP_OPENCORE_AMR_INCLUDES)
        AC_SUBST(UNIMRCP_OPENCORE_AMR_LIBS)
    fi
])
