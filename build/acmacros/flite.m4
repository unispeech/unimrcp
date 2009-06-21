dnl UNIMRCP_CHECK_FLITE

AC_DEFUN([UNIMRCP_CHECK_FLITE],
[  
    AC_MSG_NOTICE([Flite library configuration])

    AC_MSG_CHECKING([for Flite])
    AC_ARG_WITH(flite,
                [  --with-flite=PATH     prefix for installed Flite or
                          path to Flite build tree],
                [flite_path=$withval],
                [flite_path="/usr/local"]
                )
    
    found_flite="no"
    
    dnl TO BE DONE

    if test x_$found_flite != x_yes; then
        AC_MSG_ERROR(Cannot find Flite - looked for flite-config:$flite_config and srcdir:$flite_srcdir in $flite_path)
    else
        AC_MSG_RESULT([$found_flite])
        AC_MSG_RESULT([$flite_version])

case "$host" in
    *darwin*)
	UNIMRCP_FLITE_LIBS="$UNIMRCP_FLITE_LIBS -framework CoreFoundation -framework SystemConfiguration"                                                                ;;
esac

        AC_SUBST(UNIMRCP_FLITE_INCLUDES)
        AC_SUBST(UNIMRCP_FLITE_LIBS)
    fi
])
