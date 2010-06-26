dnl Initialization
AC_PREREQ(2.58)
AC_INIT([gtkhtml], [3.31.4], [http://bugzilla.gnome.org/enter_bug.cgi?product=GtkHtml])
AM_INIT_AUTOMAKE([gnu 1.9])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_SRCDIR(README)

PACKAGE=gtkhtml

dnl Required Package Versions
m4_define([gtk_minimum_version], [2.20.0])
m4_define([gail_minimum_version], [1.1.0])
m4_define([enchant_minimum_version], [1.1.7])
m4_define([gnome_icon_theme_minimum_version], [2.22.0])

GTKHTML_API_VERSION=3.14
AC_DEFINE_UNQUOTED(GTKHTML_API_VERSION, "${GTKHTML_API_VERSION}", [GtkHTML API Version])
AC_SUBST(GTKHTML_API_VERSION)

EDITOR_API_VERSION=3.14
AC_DEFINE_UNQUOTED(EDITOR_API_VERSION, "${EDITOR_API_VERSION}", [Editor API Version])
AC_SUBST(EDITOR_API_VERSION)

GTKHTML_RELEASE_STRING=${PACKAGE}-${GTKHTML_API_VERSION}
AC_DEFINE_UNQUOTED(GTKHTML_RELEASE_STRING, "${GTKHTML_RELEASE_STRING}", [GtkHTML Release String])
AC_SUBST(GTKHTML_RELEASE_STRING)

GETTEXT_PACKAGE=${GTKHTML_RELEASE_STRING}
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Package name for gettext])

I18N_LIB_CFLAGS="-DGNOME_EXPLICIT_TRANSLATION_DOMAIN=\\\"${GTKHTML_RELEASE_STRING}\\\""
AC_SUBST(I18N_LIB_CFLAGS)

dnl Automake 1.11 - Silent Build Rules
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_MAINTAINER_MODE

dnl ************
dnl I18N stuff
dnl ************
IT_PROG_INTLTOOL([0.36.3])
AM_GLIB_GNU_GETTEXT

dnl **********************************
dnl Compiler Warning Flags
dnl **********************************
AS_COMPILER_FLAGS(WARNING_FLAGS,
	"-DG_DISABLE_DEPRECATED
	-DPANGO_DISABLE_DEPRECATED
	-DGDK_DISABLE_DEPRECATED
	-DGDK_PIXBUF_DISABLE_DEPRECATED
	-DGTK_DISABLE_DEPRECATED
	-DGNOME_DISABLE_DEPRECATED
	-DG_DISABLE_SINGLE_INCLUDES
	-DGTK_DISABLE_SINGLE_INCLUDES
	-Wall -Wextra
	-Wno-missing-field-initializers
	-Wno-sign-compare
	-Wno-unused-parameter
	-Wdeclaration-after-statement
	-Werror-implicit-function-declaration
	-Wformat-nonliteral -Wformat-security -Winit-self
	-Wmissing-declarations -Wmissing-include-dirs
	-Wmissing-noreturn -Wnested-externs -Wpointer-arith
	-Wredundant-decls -Wundef -Wwrite-strings")
AC_SUBST(WARNING_FLAGS)

dnl Other useful compiler warnings for test builds only.
dnl These may produce warnings we have no control over,
dnl or false positives we don't always want to see.
dnl
dnl	-Wmissing-format-attribute
dnl	-Wshadow
dnl	-Wstrict-aliasing=2

dnl GCC 4.4 got more aggressive in its aliasing optimizations, changing
dnl behavior that -- according to the C99 standard -- is supposed to be
dnl undefined.  We may still have aliasing abuses lying around that rely
dnl on GCC's previous "undefined" behavior, so disable strict-aliasing
dnl optimization until we can find and fix all the abuses.
dnl XXX This really belongs in AM_CFLAGS.
CFLAGS="$CFLAGS $WARNING_FLAGS -fno-strict-aliasing"

dnl *********************
dnl Necessary programs
dnl *********************
AC_PROG_CC
AC_PROG_CXX
AC_C_INLINE
AC_FUNC_ALLOCA
AC_FUNC_MALLOC
AC_TYPE_SIZE_T

AC_CHECK_FUNCS([memchr memmove memset regcomp setlocale strchr strcspn strrchr strstr strtol strtoull])
AC_CHECK_HEADERS([fcntl.h libintl.h])

dnl *****************
dnl libtool
dnl *****************
LT_PREREQ(2.2)
LT_INIT(disable-static win32-dll)

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

dnl ********
dnl Win32
dnl ********
AC_MSG_CHECKING([for Win32])
case "$host" in
*-mingw*)
    os_win32=yes
    SOEXT='.dll'
    ;;
*)  os_win32=no
    SOEXT='.so'
    ;;
esac
AC_MSG_RESULT([$os_win32])
AM_CONDITIONAL(OS_WIN32, [test $os_win32 = yes])
AC_SUBST(SOEXT)

AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)

dnl **********************************
dnl GAIL modules
dnl **********************************
GAIL_MODULES="gail >= gail_minimum_version"
PKG_CHECK_MODULES(GAIL, $GAIL_MODULES)
AC_SUBST(GAIL_CFLAGS)
AC_SUBST(GAIL_LIBS)
AC_SUBST(GAIL_MODULES)

dnl **********************************
dnl GTHREAD modules
dnl **********************************
GTHREAD_MODULES="gthread-2.0"
PKG_CHECK_MODULES(GTHREAD, $GTHREAD_MODULES)
AC_SUBST(GTHREAD_CFLAGS)
AC_SUBST(GTHREAD_LIBS)

dnl **********************************
dnl GTKHTML modules
dnl **********************************
GTKHTML_MODULES="gtk+-2.0 >= gtk_minimum_version enchant >= enchant_minimum_version iso-codes libxml-2.0 libcroco-0.6"
PKG_CHECK_MODULES(GTKHTML, $GTKHTML_MODULES)
AC_SUBST(GTKHTML_CFLAGS)
AC_SUBST(GTKHTML_LIBS)
AC_SUBST(GTKHTML_MODULES)

dnl **********************************
dnl Glade catalog files
dnl **********************************
AC_ARG_WITH(glade-catalog,
	AC_HELP_STRING([--with-glade-catalog],
	[install the catalog files for Glade 3]
	[(for maintainers only) @<:@default=no@:>@]),
	[glade_catalog="$withval"], [glade_catalog="no"])
if test "x$glade_catalog" = "xyes"; then
	PKG_CHECK_MODULES(GLADEUI, gladeui-1.0)
fi
AM_CONDITIONAL(GLADE_CATALOG, test x$glade_catalog = xyes)

dnl These are still needed for 'dist' targets.
ORBIT_IDL="`$PKG_CONFIG --variable=orbit_idl ORBit-2.0`"
AC_SUBST(ORBIT_IDL)

dnl **********************************
dnl soup for testgtkhtml
dnl **********************************
HAVE_NEWSOUP="no"
have_newsoup="no"
PKG_CHECK_MODULES(SOUP, libsoup-2.4  >= 2.26.0,
	[	HAVE_NEWSOUP="yes"
		have_soup="yes"
	],
	[	PKG_CHECK_MODULES(SOUP, libsoup-2.4,
		have_soup="yes",
		have_soup="no")
	])
AC_SUBST(HAVE_NEWSOUP)
AM_CONDITIONAL(HAVE_SOUP, test x$have_soup != xno)
AM_CONDITIONAL(HAVE_NEWSOUP, test x$HAVE_NEWSOUP != xno)

dnl **********************************
dnl gconf for default settings
dnl **********************************

PKG_CHECK_MODULES(GCONF,  gconf-2.0,
    HAVE_GCONF="yes",
    HAVE_GCONF="no")

AC_SUBST(HAVE_GCONF)
AM_CONDITIONAL(HAVE_GCONF, test x$HAVE_GCONF != xno)

dnl **************************************************
dnl Gnome Icon Theme
dnl **************************************************
PKG_CHECK_MODULES(GIT, gnome-icon-theme >= gnome_icon_theme_minimum_version)

dnl **************************************************
dnl iso-codes
dnl **************************************************
AC_MSG_CHECKING([for iso-codes package])
have_iso_codes=no
if $PKG_CONFIG --exists iso-codes; then
	if $PKG_CONFIG iso-codes --atleast-version=0.49; then
		have_iso_codes=yes
		AC_MSG_RESULT([$have_iso_codes])
	else
		AC_MSG_WARN([iso-codes detected, but version 0.49 or later is required due to licensing])
	fi
else
	AC_MSG_RESULT([$have_iso_codes])
fi

if test "x$have_iso_codes" = "xyes"; then
	AC_MSG_CHECKING([whether iso-codes has iso-639 and iso-3166 domains])
	if $PKG_CONFIG --variable=domains iso-codes | grep 639 >/dev/null 2>&1 && \
		$PKG_CONFIG --variable=domains iso-codes | grep 3166 >/dev/null 2>&1 ; then
			result=yes
	else
		result=no
		have_iso_codes=no
	fi
	AC_MSG_RESULT([$result])
fi

if test "x$have_iso_codes" = "xyes"; then
	AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],
		["`$PKG_CONFIG --variable=prefix iso-codes`"],
		[ISO codes prefix])
	AC_DEFINE([HAVE_ISO_CODES], [1],
		[Define if you have the iso-codes package])
fi

dnl **************************************************
dnl XF86 multimedia keys support
dnl **************************************************
AC_CHECK_HEADER([X11/XF86keysym.h],
	[AC_DEFINE([HAVE_XFREE], 1, [Have <X11/XF86keysym.h>])],,
	[[	#if HAVE_XFREE
		#include <X11/XF86keysym.h>
		#endif
	]])

dnl **************************************************
dnl regex library
dnl **************************************************
AC_CHECK_FUNCS(regexec,,[AC_CHECK_LIB(regex,regexec,[REGEX_LIBS=-lregex],[AC_MSG_ERROR([No regex library found])])])
AC_SUBST(REGEX_LIBS)

dnl ******************************
dnl shlib factory
dnl ******************************
AC_ARG_ENABLE([shlib-factory],
		AS_HELP_STRING([--enable-shlib-factory],
		[build editor factory as shared library @<:@default=yes@:>@]),
		[enable_shlib_factory="$enableval"],
		[enable_shlib_factory="yes"])

if test "x$enable_shlib_factory" = "xyes"; then
	GNOME_GTKHTML_EDITOR_SHLIB=1
	GNOME_GTKHTML_EDITOR_TYPE="shlib"
	AC_DEFINE(GNOME_GTKHTML_EDITOR_SHLIB,, [Editor shlib])
else
	GNOME_GTKHTML_EDITOR_TYPE="exe"
fi

AC_SUBST(GNOME_GTKHTML_EDITOR_TYPE)
AM_CONDITIONAL(EDITOR_SHLIB, test x$enable_shlib_factory = xyes)

dnl **********************************
dnl Library version information.
dnl **********************************

dnl Increment the following if the interface has additions, changes,
dnl removals.
GTKHTML_CURRENT=20

dnl Increment any time the source changes; set to 0 if you
dnl increment CURRENT.
GTKHTML_REVISION=1

dnl Increment if any interfaces have been added; set to 0
dnl if any interfaces have been removed. removal has
dnl precedence over adding, so set to 0 if both happened.
GTKHTML_AGE=1

AC_SUBST(GTKHTML_CURRENT)
AC_SUBST(GTKHTML_REVISION)
AC_SUBST(GTKHTML_AGE)

dnl **************
dnl Done.
dnl **************

AC_CONFIG_FILES([
Makefile
gtkhtml-zip
a11y/Makefile
art/Makefile
components/Makefile
components/editor/Makefile
components/editor/gtkhtml-editor.pc
gtkhtml/Makefile
gtkhtml/libgtkhtml.pc
po/Makefile.in
])
AC_OUTPUT

echo "
Configuration:

	Cookies support in soup :		${HAVE_NEWSOUP}
	Support gconf           :		${HAVE_GCONF}
	Editor component type   :		${GNOME_GTKHTML_EDITOR_TYPE}
"
