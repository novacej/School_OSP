
dnl Check the header
AC_DEFUN([MC_CHECK_SLANG_HEADER], [
    AC_MSG_CHECKING([for slang/slang.h])
    AC_PREPROC_IFELSE(
        [
            AC_LANG_PROGRAM([#include <slang/slang.h>],[return 0;])
        ],
        [
            AC_MSG_RESULT(yes)
            AC_DEFINE(HAVE_SLANG_SLANG_H, 1,[Define to use slang.h])
            found_slang=yes
        ],
        [
            AC_MSG_RESULT(no)
        ]
    )
])

dnl
dnl Check if the system S-Lang library can be used.
dnl If not, and $1 is "strict", exit.
dnl
AC_DEFUN([MC_CHECK_SLANG_BY_PATH], [

    param_slang_inc_path=[$1]
    param_slang_lib_path=[$2]

    if test x"$param_slang_inc_path" != x; then
        ac_slang_inc_path="-I"$param_slang_inc_path
    fi

    if test x"$param_slang_lib_path" != x; then
        ac_slang_lib_path="-L"$param_slang_lib_path
    fi

    saved_CFLAGS="$CFLAGS"
    saved_CPPFLAGS="$CPPFLAGS"
    saved_LDFLAGS="$LDFLAGS"

    CFLAGS="$CFLAGS $ac_slang_inc_path $ac_slang_lib_path"
    CPPFLAGS="$saved_CPPFLAGS $ac_slang_inc_path $ac_slang_lib_path"

    AC_MSG_CHECKING([for slang.h])
    AC_PREPROC_IFELSE(
	[
	    AC_LANG_PROGRAM([#include <slang.h>],[return 0;])
	],
	[
	    AC_MSG_RESULT(yes)
	    if test x"$ac_slang_inc_path" = x; then
		ac_slang_inc_path="-I/usr/include"
	    fi
	    if test x"$ac_slang_lib_path" = x; then
		ac_slang_lib_path="-L/usr/lib"
	    fi
	    found_slang=yes
	    AC_DEFINE(HAVE_SLANG_H, 1,[Define to use slang.h])

	],
	[
	    AC_MSG_RESULT(no)

	    MC_CHECK_SLANG_HEADER
	    if test x"$found_slang" = xno; then
		error_msg_slang="Slang header not found"
	    else
		if test x"$ac_slang_inc_path" = x; then
		    ac_slang_inc_path="-I/usr/include"
		fi
		if test x"$ac_slang_lib_path" = x; then
		    ac_slang_lib_path="-L/usr/lib"
		fi
		CFLAGS="-DHAVE_SLANG_SLANG_H $CFLAGS"
	    fi
	],
    )
    dnl check if S-Lang have version 2.0 or newer
    if test x"$found_slang" = x"yes"; then
        AC_MSG_CHECKING([for S-Lang version 2.0 or newer])
        AC_RUN_IFELSE([
#ifdef HAVE_SLANG_SLANG_H
#include <slang/slang.h>
#else
#include <slang.h>
#endif
int main (void)
{
#if SLANG_VERSION >= 20000
    return 0;
#else
    return 1;
#endif
}
],
	    [mc_slang_is_valid_version=yes],
	    [mc_slang_is_valid_version=no],
	    [
	    if test -f "$param_slang_inc_path/slang/slang.h" ; then
	        hdr_file="$param_slang_inc_path/slang/slang.h"
	    else
	        hdr_file="$param_slang_inc_path/slang.h"
	    fi
	    mc_slang_is_valid_version=`grep '^#define SLANG_VERSION[[:space:]]' "$hdr_file"| sed s'/^#define SLANG_VERSION[[:space:]]*//'`
	    if test "$mc_slang_is_valid_version" -ge "20000"; then
		mc_slang_is_valid_version=yes
	    else
		mc_slang_is_valid_version=no
	    fi
	    ]
	)
	if test x$mc_slang_is_valid_version = xno; then
            found_slang=no
            error_msg_slang="S-Lang library version 2.0 or newer not found"
	fi
	AC_MSG_RESULT($mc_slang_is_valid_version)
    fi
    dnl Check if termcap is needed.
    dnl This check must be done before anything is linked against S-Lang.
    if test x"$found_slang" = x"yes"; then
        MC_SLANG_TERMCAP
        if test x"$mc_cv_slang_termcap"  = x"yes"; then
	    saved_CPPFLAGS="-ltermcap $saved_CPPFLAGS "
	    saved_LDFLAGS="-ltermcap $saved_LDFLAGS"
        fi


        dnl Check the library
	unset ac_cv_lib_slang_SLang_init_tty
        AC_CHECK_LIB(
            [slang],
            [SLang_init_tty],
            [:],
            [
                found_slang=no
                error_msg_slang="S-lang library not found"
            ]
        )
    fi

    dnl Unless external S-Lang was requested, reject S-Lang with UTF-8 hacks
    if test x"$found_slang" = x"yes"; then
	unset ac_cv_lib_slang_SLsmg_write_nwchars
        AC_CHECK_LIB(
            [slang],
            [SLsmg_write_nwchars],
            [
                found_slang=no
                error_msg_slang="Rejecting S-Lang with UTF-8 support, it's not fully supported yet"
            ],
            [:]
        )
    fi

    if test x"$found_slang" = x"yes"; then
        screen_type=slang
        screen_msg="S-Lang library (installed on the system)"

        MCLIBS="$ac_slang_lib_path -lslang $MCLIBS"
        CFLAGS="$ac_slang_inc_path $saved_CFLAGS"
        dnl do not reset CPPFLAGS
        dnl - if CPPFLAGS are resetted then cpp does not find the specified header
        LDFLAGS="$saved_LDFLAGS"
    else
        CFLAGS="$saved_CFLAGS"
        CPPFLAGS="$saved_CPPFLAGS"
        LDFLAGS="$saved_LDFLAGS"
    fi
])

dnl
dnl Use the slang library.
dnl
AC_DEFUN([MC_WITH_SLANG], [
    with_screen=slang
    found_slang=yes
    error_msg_slang=""

    AC_ARG_WITH([slang-includes],
        AC_HELP_STRING([--with-slang-includes=@<:@DIR@:>@],
            [set path to SLANG includes @<:@default=/usr/include@:>@; make sense only if --with-screen=slang]
        ),
        [ac_slang_inc_path="$withval"],
        [ac_slang_inc_path=""]
    )

    AC_ARG_WITH([slang-libs],
        AC_HELP_STRING([--with-slang-libs=@<:@DIR@:>@],
            [set path to SLANG library @<:@default=/usr/lib@:>@; make sense only if --with-screen=slang]
        ),
        [ac_slang_lib_path="$withval"],
        [ac_slang_lib_path=""]
    )
    if test x"$ac_slang_lib_path" != x -o x"$ac_slang_inc_path" != x; then
        echo 'checking SLANG-headers in specified place ...'
        MC_CHECK_SLANG_BY_PATH([$ac_slang_inc_path],[$ac_slang_lib_path])
    else
        found_slang=no
        PKG_CHECK_MODULES(SLANG, [slang >= 2.0], [found_slang=yes], [:])
        if test x"$found_slang" = "xyes"; then
            MCLIBS="$pkg_cv_SLANG_LIBS $MCLIBS"
            CFLAGS="$pkg_cv_SLANG_CFLAGS $CFLAGS"
        fi
    fi

    if test x"$found_slang" = "xno"; then
        found_slang=yes
        ac_slang_inc_path="/usr/include"
        ac_slang_lib_path="/usr/lib"

        echo 'checking SLANG-headers in /usr ...'
        MC_CHECK_SLANG_BY_PATH([$ac_slang_inc_path],[$ac_slang_lib_path])
        if test x"$found_slang" = "xno"; then
            found_slang=yes
            ac_slang_inc_path="/usr/local/include"
            ac_slang_lib_path="/usr/local/lib"

            echo 'checking SLANG-headers in /usr/local ...'
            MC_CHECK_SLANG_BY_PATH( $ac_slang_inc_path , $ac_slang_lib_path )
            if test x"$found_slang" = "xno"; then
                AC_MSG_ERROR([$error_msg_slang])
            fi
        fi
    fi

    AC_DEFINE(HAVE_SLANG, 1,
        [Define to use S-Lang library for screen management])

    MC_CHECK_SLANG_HEADER

])
