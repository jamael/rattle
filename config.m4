AC_INIT([rattd], [0.0.0])
AC_DEFINE([RATTLE_VERSION_MAJOR], [0], [RATTLE major number])
AC_DEFINE([RATTLE_VERSION_MINOR], [0], [RATTLE minor number])
AC_DEFINE([RATTLE_VERSION_PATCH], [0], [RATTLE patch level])

AC_CONFIG_SRCDIR([LICENSE])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_FILES([Makefile
		modules/Makefile
		include/Makefile])

AM_INIT_AUTOMAKE([foreign subdir-objects])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_DECL_SYS_SIGLIST

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_TYPE_UINT32_T
AC_TYPE_UINT16_T
AC_TYPE_UINT8_T

# Checks for dynamic library loader
LT_LIB_DLLOAD

# Use libconfig for configuration file syntax
PKG_CHECK_MODULES([libconfig], [libconfig >= 1.4.5],,
	AC_MSG_ERROR([libconfig 1.4.5 or newer not found.])
)
AC_SUBST(libconfig_CFLAGS)
AC_SUBST(libconfig_LIBS)

# --enable-debug
AC_ARG_ENABLE([debug],
	[AS_HELP_STRING([--enable-debug],
		[enable debugging prints and trace])])
AS_IF([test "x$enable_debug" == "xyes"],
	[AC_DEFINE([DEBUG], [1],
		[Define if you want debug code])])

# --enable-test-mode
AC_ARG_ENABLE([test-mode],
	[AS_HELP_STRING([--enable-test-mode],
		[enable benchmark and various tests])])
AS_IF([test "x$enable_test_mode" == "xyes"],
	[AC_DEFINE([WANT_TEST], [1],
		[Define if you want test mode])])
AM_CONDITIONAL([WANT_TEST], [test "x$enable_test_mode" != "xno"])

AC_DEFINE([WANT_THREADS], [1], [threads])

m4_include([m4/rattle.m4])
