dnl
dnl UNIMRCP_CHECK_VO_AMRWBENC
dnl
dnl This macro attempts to find the VO AMRWBENC library and
dnl set corresponding variables on exit.
dnl
AC_DEFUN([UNIMRCP_CHECK_VO_AMRWBENC],
[
    AC_MSG_NOTICE([VO AMRWBENC library configuration])

    AC_MSG_CHECKING([for VO AMRWBENC])
    AC_ARG_WITH(vo-amrwbenc,
                [  --with-vo-amrwbenc=PATH   prefix for installed VO AMRWBENC,
                          or the full path to VO AMRWBENC pkg-config],
                [vo_amrwbenc_path=$withval],
                [vo_amrwbenc_path="/usr/local/vo-amrwbenc"]
                )

    found_vo_amrwbenc="no"

    if test -n "$PKG_CONFIG"; then
        dnl Check for installed VO AMRWBENC
        for dir in $vo_amrwbenc_path ; do
            vo_amrwbenc_config_path=$dir/lib/pkgconfig/vo-amrwbenc.pc
            if test -f "$vo_amrwbenc_config_path" && $PKG_CONFIG $vo_amrwbenc_config_path > /dev/null 2>&1; then
                found_vo_amrwbenc="yes"
                break
            fi
        done

        dnl Check for full path to VO AMRWBENC pkg-config file
        if test "$found_vo_amrwbenc" != "yes" && test -f "$vo_amrwbenc_path" && $PKG_CONFIG $vo_amrwbenc_path > /dev/null 2>&1 ; then
            found_vo_amrwbenc="yes"
            vo_amrwbenc_config_path=$vo_amrwbenc_path
        fi

        if test "$found_vo_amrwbenc" = "yes" ; then
            UNIMRCP_VO_AMRWBENC_INCLUDES="`$PKG_CONFIG --cflags $vo_amrwbenc_config_path`"
            UNIMRCP_VO_AMRWBENC_LIBS="`$PKG_CONFIG --libs $vo_amrwbenc_config_path`"
            vo_amrwbenc_version="`$PKG_CONFIG --modversion $vo_amrwbenc_config_path`"
        fi
    fi

    if test $found_vo_amrwbenc != "yes" ; then
        AC_MSG_ERROR(Cannot find VO AMRWBENC - looked in $vo_amrwbenc_path)
    else
        AC_MSG_RESULT([$found_vo_amrwbenc])
        AC_MSG_RESULT([$vo_amrwbenc_version])

case "$host" in
    *darwin*)
        UNIMRCP_VO_AMRWBENC_LIBS="$UNIMRCP_VO_AMRWBENC_LIBS -framework CoreFoundation -framework SystemConfiguration"
        ;;
esac

        AC_SUBST(UNIMRCP_VO_AMRWBENC_INCLUDES)
        AC_SUBST(UNIMRCP_VO_AMRWBENC_LIBS)
    fi
])
