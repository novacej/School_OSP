dnl
dnl Configure.in file for the Midnight Commander
dnl

AC_INIT([GNU Midnight Commander], [], [mc-devel@gnome.org])
AC_PREREQ(2.60)
m4_pattern_forbid(MC_)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_AUX_DIR(config)
MC_VERSION
AM_INIT_AUTOMAKE(mc, ${VERSION} )

dnl Enable silent rules by default (if yes)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE
AC_CANONICAL_HOST

AC_USE_SYSTEM_EXTENSIONS

AC_PROG_LIBTOOL
PKG_PROG_PKG_CONFIG
AC_PATH_PROG([PERL], [perl], [/usr/bin/perl])

AC_ISC_POSIX

AC_PROG_CC_STDC
AM_PROG_CC_C_O

dnl Doxygen

DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN(mc,doxygen.cfg,devel)

dnl PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])

dnl
dnl First try glib 2.x.
dnl Keep this check close to the beginning, so that the users
dnl without any glib won't have their time wasted by other checks.
dnl

AC_ARG_WITH([glib_static],
	    [  --with-glib-static       Link glib statically [[no]]])

glib_found=no

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.8], [glib_found=yes], [:])
if test x"$glib_found" = xno; then
    AC_MSG_ERROR([glib-2.0 not found or version too old (must be >= 2.8)])
fi


AC_HEADER_MAJOR
AC_C_CONST
AC_SYS_LARGEFILE

AC_PROG_LN_S
AC_CHECK_TOOL(AR, ar, ar)

dnl Only list browsers here that can be run in background (i.e. with `&')
AC_CHECK_PROGS(X11_WWW, [gnome-moz-remote mozilla konqueror opera netscape])

dnl
dnl Ovverriding mmap support.  This has to be before AC_FUNC_MMAP is used.
dnl We use only part of the functionality of mmap, so on AIX,
dnl it's possible to use mmap, even if it doesn't pass the autoconf test.
dnl
AC_ARG_WITH(mmap,
	[  --with-mmap              Use the mmap call [[yes if found]]])
if test x$with_mmap != xno; then
    if test x$with_mmap = x; then
	AC_FUNC_MMAP
    else
	AC_DEFINE(HAVE_MMAP, 1)
    fi
fi

dnl
dnl Internationalization
dnl
AM_GNU_GETTEXT(no-libtool, need-ngettext)
AM_GNU_GETTEXT_VERSION(0.14.3)

if test "x$USE_INCLUDED_LIBINTL" = xyes; then
    CPPFLAGS="$CPPFLAGS -I\$(top_builddir)/intl -I\$(top_srcdir)/intl"
fi

dnl Determine which help translations we want to install.
ALL_DOC_LINGUAS="es hu it pl ru sr"

DOC_LINGUAS=
if test "x$USE_NLS" = xyes; then
    if test -z "$LINGUAS"; then
	langs="`grep -v '^#' $srcdir/po/LINGUAS`"
    else
	langs="$LINGUAS"
    fi
else
    langs=
fi

for h_lang in $ALL_DOC_LINGUAS; do
    for lang in $langs; do
	if test "$lang" = "$h_lang"; then
	    DOC_LINGUAS="$DOC_LINGUAS $lang"
	    break
	fi
    done
done
AC_SUBST(DOC_LINGUAS)

dnl
dnl OS specific flags.
dnl
case $host_os in
aux*)
    # A/UX
    LIBS="$LIBS -lposix"
    AC_DEFINE(_POSIX_SOURCE)
    ;;
esac

AC_PROG_INSTALL
AC_CHECK_HEADERS([unistd.h string.h memory.h limits.h malloc.h \
	utime.h fcntl.h sys/statfs.h sys/vfs.h sys/time.h \
	sys/timeb.h sys/select.h sys/ioctl.h stropts.h arpa/inet.h \
	security/pam_misc.h sys/socket.h sys/sysmacros.h sys/types.h \
	sys/mkdev.h wchar.h wctype.h])

AC_HEADER_TIME
AC_HEADER_DIRENT
AC_HEADER_STDC

dnl Missing structure components
AC_CHECK_MEMBERS([struct stat.st_blksize, struct stat.st_rdev])
AC_STRUCT_ST_BLOCKS

dnl
dnl Check availability of some functions
dnl

AC_CHECK_FUNCS([\
	atoll \
	cfgetospeed \
	initgroups isascii \
	setreuid statfs sysconf \
	tcgetattr tcsetattr truncate \
	strverscmp \
	strncasecmp \
	realpath
])

dnl
dnl getpt is a GNU Extension (glibc 2.1.x)
dnl
AC_CHECK_FUNCS(posix_openpt, , [AC_CHECK_FUNCS(getpt)])
AC_CHECK_FUNCS(grantpt, , [AC_CHECK_LIB(pt, grantpt)])

dnl replacing lstat with statlstat on sco makes it more portable between
dnl sco clones
AC_CHECK_FUNCS(statlstat)

dnl
dnl If running under AIX, AC_AIX does not tell us that
dnl
AC_MSG_CHECKING([for AIX defines])
AC_EGREP_CPP(yes,
[#if defined(AIX) || defined(_AIX) || defined(__aix__) || defined(aix)
yes
#endif
], [
AC_DEFINE(IS_AIX, 1, [Define if compiling for AIX])
AC_MSG_RESULT(yes)
], [AC_MSG_RESULT(no)])

dnl
dnl This is from GNU fileutils, check aclocal.m4 for more information
dnl
AC_GET_FS_INFO

dnl
dnl Missing typedefs and replacements
dnl

AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_CHECK_SIZEOF(off_t)
AC_TYPE_PID_T
AC_TYPE_UID_T
AC_CHECK_TYPE(nlink_t, unsigned int)
AC_CHECK_TYPES([socklen_t],,,
[
#include <sys/types.h>
#include <sys/socket.h>
])

dnl This is needed for regex.c only
AC_CHECK_TYPE(uintptr_t,
	      [AC_DEFINE(HAVE_UINTPTR_T, 1,
			 [Define if you have the `uintptr_t' type.])
])

AC_FUNC_ALLOCA
AC_FUNC_STRCOLL

dnl
dnl X11 support.
dnl Used to read keyboard modifiers when running under X11.
AC_PATH_XTRA
dnl
dnl Check if the gmodule functionality supported on this system.
AC_G_MODULE_SUPPORTED

dnl
dnl Try to find static libraries for glib and gmodule.
dnl
if test x$with_glib_static = xyes; then
    new_GLIB_LIBS=
    for i in $GLIB_LIBS; do
	case x$i in
	x-lglib*)
	    lib=glib ;;
	x-lgmodule*)
	    lib=gmodule ;;
	*)
	    lib=
	    add="$i" ;;
	esac

	if test -n "$lib"; then
	    lib1=`echo $i | sed 's/^-l//'`
	    if test -f "$GLIB_LIBDIR/lib${lib1}.a"; then
		add="$GLIB_LIBDIR/lib${lib1}.a"
	    else
		if test -f "$GLIB_LIBDIR/lib${lib}.a"; then
		    add="$GLIB_LIBDIR/lib${lib}.a"
		else
		    AC_MSG_ERROR([Cannot find static $lib])
		fi
	    fi
	fi
	new_GLIB_LIBS="$new_GLIB_LIBS $add"
    done
    GLIB_LIBS="$new_GLIB_LIBS"
fi

dnl
dnl Sequent wants getprocessstats
dnl
AC_CHECK_LIB(seq, get_process_stats, [
	LIBS="$LIBS -lseq"
	AC_DEFINE(HAVE_GET_PROCESS_STATS, 1,
		  [Define if you have function `get_process_stats' and
have to use that instead of gettimeofday])])

AC_MC_VFS_CHECKS

dnl
dnl Check for gpm mouse support (Linux only)
dnl
mouse_lib="xterm only"
AC_ARG_WITH(gpm-mouse,
	[  --with-gpm-mouse         Compile with gpm mouse support (Linux only)
                           [[yes if found]]])

case $host_os in
linux*)
    if test x$with_gpm_mouse != xno; then
	AC_CHECK_LIB(gpm, Gpm_Repeat,
	    [AC_DEFINE(HAVE_LIBGPM, 1,
		       [Define to enable gpm mouse support on Linux])
	    mouse_lib="gpm and xterm"
	    MCLIBS="$MCLIBS -lgpm"],
            if test "x$with_gpm_mouse" = "xyes"; then
                [AC_MSG_ERROR([libgpm is missing or older than 0.18])]
            else
                [AC_MSG_WARN([libgpm is missing or older than 0.18])]
            fi
	)
    fi
    ;;
esac

MC_CHECK_SEARCH_TYPE

dnl
dnl Check nroff and the options it supports
dnl
AC_CHECK_PROG(HAVE_nroff, nroff, true, false)

dnl Default values
MANDOC=-man
MAN_FLAGS=

if $HAVE_nroff; then
    AC_MSG_CHECKING([for manual formatting macros])
    AC_CACHE_VAL(mc_cv_mandoc, [
    nroff -mandoc < /dev/null > /dev/null 2>&1
    if test $? = 0; then
	mc_cv_mandoc=-mandoc
    else
	mc_cv_mandoc=-man
    fi
    ])
    MANDOC=$mc_cv_mandoc
    AC_MSG_RESULT([$MANDOC])

    AC_MSG_CHECKING([for option to disable ANSI color in manuals])
    AC_CACHE_VAL(mc_cv_man_nocolor, [
    nroff -c < /dev/null > /dev/null 2>&1
    if test $? = 0; then
	mc_cv_man_nocolor=-c
    else
	mc_cv_man_nocolor=
    fi
    ])
    MAN_FLAGS=$mc_cv_man_nocolor
    AC_MSG_RESULT([${MAN_NOCOLOR-none}])

    AC_MSG_CHECKING([if nroff accepts -Tlatin1 or -Tascii])
    AC_CACHE_VAL(mc_cv_nroff_tascii, [
    mc_cv_nroff_tascii=
    nroff -Tlatin1 < /dev/null > /dev/null 2>&1 /dev/null
    if test $? = 0; then
	mc_cv_nroff_tascii=-Tlatin1
    else
        nroff -Tascii < /dev/null > /dev/null 2>&1 /dev/null
        if test $? = 0; then
	    mc_cv_nroff_tascii=-Tascii
	fi
    fi
    ])
    AC_MSG_RESULT([${mc_cv_nroff_tascii-no}])
    MAN_FLAGS="$MAN_FLAGS $mc_cv_nroff_tascii"
fi

AC_SUBST(MANDOC)
AC_SUBST(MAN_FLAGS)


dnl
dnl Check for -L option to file
dnl
AC_CHECK_PROG(HAVE_FILECMD, file, true, false)
if $HAVE_FILECMD; then
    AC_MSG_CHECKING([for -L option to file command])
    AC_CACHE_VAL(mc_cv_filel, [
    file -L . > /dev/null 2>&1
    if test $? = 0; then
	mc_cv_filel=yes
    else
	mc_cv_filel=no
    fi
    ])
    if test x$mc_cv_filel = xyes; then
	AC_DEFINE(FILE_L, 1, [Define if the file command accepts the -L option])
    fi
    filel=$mc_cv_filel
    AC_MSG_RESULT([$filel])
fi


AC_MSG_CHECKING([for subshell support])
AC_ARG_WITH(subshell,
	[  --with-subshell          Compile in concurrent subshell [[yes]]
  --with-subshell=optional Don't run concurrent shell by default [[no]]],
	[result=no
	if test x$withval = xoptional
	then
		AC_DEFINE(SUBSHELL_OPTIONAL, 1,
			  [Define to make subshell support optional])
		result="optional"
	fi
	if test x$withval = xyes
        then
		result="yes"
	fi],
	[dnl Default: enable the subshell support
	result="yes"
])
if test "x$result" != xno; then
	AC_DEFINE(HAVE_SUBSHELL_SUPPORT, 1,
		  [Define to enable subshell support])
fi
AC_MSG_RESULT([$result])
subshell="$result"


MC_WITH_SCREEN


dnl
dnl Internal editor support.
dnl
AC_ARG_WITH(edit,
        [  --with-edit              Enable internal editor [[yes]]])

if test x$with_edit != xno; then
	AC_DEFINE(USE_INTERNAL_EDIT, 1, [Define to enable internal editor])
	use_edit=yes
	edit_msg="yes"
	AC_MSG_NOTICE([using internal editor])
else
	edit_msg="no"
fi

dnl
dnl Diff viewer support.
dnl
AC_ARG_WITH(diff_viewer,
        [  --with-diff-viewer       Compile with diff viewer [[yes]]])

if test x$with_diff_viewer != xno; then
	AC_DEFINE(USE_DIFF_VIEW, 1, [Define to enable diff viewer])
	use_diff=yes
	diff_msg="yes"
	AC_MSG_NOTICE([using diff viewer])
else
	diff_msg="no"
fi


dnl Check if the OS is supported by the console saver.
cons_saver=""
case $host_os in
linux*)
    cons_saver=yes
esac


dnl Support for background operations
AC_ARG_ENABLE([background],
	      [  --enable-background     Support for background file operations [[yes]]])
if test "x$enable_background" != xno; then
    AC_DEFINE(WITH_BACKGROUND, 1, [Define to enable background file operations])
fi


dnl
dnl User visible support for charset conversion.
dnl
AC_ARG_ENABLE([charset],
	      [  --enable-charset        Support for charset selection and conversion [[yes]]])
have_charset=
charset_msg="no"
if test "x$enable_charset" != "xno"; then
    AC_DEFINE(HAVE_CHARSET, 1,
	      [Define to enable charset selection and conversion])
    have_charset=yes
    charset_msg="yes"
fi

if test "$GLIBC21" != yes; then
    AC_DEFINE(USE_INCLUDED_REGEX, 1, [Use the regex included here])
fi

MC_CHECK_CFLAGS

CFLAGS_OPTS=""

if test "x$CFLAGS" = "x"; then
    CFLAGS_OPTS=" -O2 "
fi

if test x$USE_MAINTAINER_MODE = xyes; then
    CFLAGS_OPTS="-g3 -O -ggdb"
    AC_DEFINE(USE_MAINTAINER_MODE, 1, [Use maintainer mode])
fi

AC_ARG_ENABLE(
    [werror],
    AC_HELP_STRING([--enable-werror], [Handle all compiler warnings as errors] )
)

if test "x$enable_werror" = xyes; then
    MC_CHECK_ONE_CFLAG([-Werror])
fi
CFLAGS="$mc_configured_cflags $CFLAGS_OPTS $CFLAGS"

AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(LIBS)

dnl Libraries used only when building the mc binary
AC_SUBST(MCLIBS)

MAN_DATE="$(LC_ALL=C date "+%B %Y")"
AC_SUBST(MAN_DATE)

AM_CONDITIONAL(USE_MAINTAINER_MODE, [test x"$USE_MAINTAINER_MODE" = xyes])
AM_CONDITIONAL(USE_SCREEN_SLANG, [test x"$with_screen" = xslang])
AM_CONDITIONAL(USE_EDIT, [test -n "$use_edit"])
AM_CONDITIONAL(USE_DIFF, [test -n "$use_diff"])
AM_CONDITIONAL(CHARSET, [test -n "$have_charset"])
AM_CONDITIONAL(CONS_SAVER, [test -n "$cons_saver"])

AC_CONFIG_FILES(
[
src/man2hlp/man2hlp
],
[
chmod +x src/man2hlp/man2hlp
])

AC_CONFIG_FILES([
Makefile

contrib/Makefile
contrib/dist/Makefile
contrib/dist/debian/Makefile
contrib/dist/gentoo/Makefile
contrib/dist/redhat/Makefile
contrib/dist/redhat/mc.spec
contrib/dist/pkginfo
contrib/dist/prototype

misc/Makefile
misc/skins/Makefile
misc/mc.ext

src/Makefile
src/consaver/Makefile
src/editor/Makefile
src/man2hlp/Makefile
src/viewer/Makefile
src/diffviewer/Makefile
src/filemanager/Makefile

lib/Makefile
lib/filehighlight/Makefile
lib/mcconfig/Makefile
lib/search/Makefile
lib/skin/Makefile
lib/strutil/Makefile
lib/tty/Makefile
lib/vfs/Makefile
lib/vfs/mc-vfs/Makefile
lib/vfs/mc-vfs/extfs/Makefile
lib/vfs/mc-vfs/extfs/a+
lib/vfs/mc-vfs/extfs/apt+
lib/vfs/mc-vfs/extfs/audio
lib/vfs/mc-vfs/extfs/deb
lib/vfs/mc-vfs/extfs/deba
lib/vfs/mc-vfs/extfs/debd
lib/vfs/mc-vfs/extfs/dpkg+
lib/vfs/mc-vfs/extfs/iso9660
lib/vfs/mc-vfs/extfs/hp48+
lib/vfs/mc-vfs/extfs/lslR
lib/vfs/mc-vfs/extfs/mailfs
lib/vfs/mc-vfs/extfs/patchfs
lib/vfs/mc-vfs/extfs/rpms+
lib/vfs/mc-vfs/extfs/s3+
lib/vfs/mc-vfs/extfs/uace
lib/vfs/mc-vfs/extfs/ualz
lib/vfs/mc-vfs/extfs/uar
lib/vfs/mc-vfs/extfs/uarc
lib/vfs/mc-vfs/extfs/uarj
lib/vfs/mc-vfs/extfs/uc1541
lib/vfs/mc-vfs/extfs/uha
lib/vfs/mc-vfs/extfs/ulha
lib/vfs/mc-vfs/extfs/urar
lib/vfs/mc-vfs/extfs/uzip
lib/vfs/mc-vfs/extfs/uzoo
lib/vfs/mc-vfs/fish/Makefile
lib/widget/Makefile

misc/syntax/Makefile

doc/Makefile
doc/hints/Makefile
doc/man/mc.1 doc/man/mcedit.1 doc/man/mcview.1 doc/man/Makefile
doc/man/es/mc.1 doc/man/es/Makefile
doc/man/hu/mc.1 doc/man/hu/Makefile
doc/man/it/mc.1 doc/man/it/Makefile
doc/man/pl/mc.1 doc/man/pl/Makefile
doc/man/ru/mc.1 doc/man/ru/Makefile
doc/man/sr/mc.1 doc/man/sr/Makefile

doc/hlp/Makefile
doc/hlp/es/Makefile
doc/hlp/hu/Makefile
doc/hlp/it/Makefile
doc/hlp/pl/Makefile
doc/hlp/ru/Makefile
doc/hlp/sr/Makefile

intl/Makefile
po/Makefile.in
])

AC_OUTPUT

echo "
Configuration:

  Source code location:       ${srcdir}
  Compiler:                   ${CC}
  Compiler flags:             ${CFLAGS}
  File system:                ${vfs_type}
                              ${vfs_flags}
  Screen library:             ${screen_msg}
  Mouse support:              ${mouse_lib}
  X11 events support:         ${textmode_x11_support}
  With subshell support:      ${subshell}
  Internal editor:            ${edit_msg}
  Diff viewer:                ${diff_msg}
  Support for charset:        ${charset_msg}
  Search type:                ${SEARCH_TYPE}
"
