AC_INIT([rattd], [0.1])

AC_CONFIG_SRCDIR(LICENSE)
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE(foreign)

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Use libconfig for configuration file syntax
PKG_CHECK_MODULES([libconfig], [libconfig >= 1.3],,
  AC_MSG_ERROR([libconfig 1.3 or newer not found.])
)
AC_SUBST(libconfig_CFLAGS)
AC_SUBST(libconfig_LIBS)

# absolute path to SRCDIR
srcroot=$(cd $srcdir; pwd)
AM_CPPFLAGS="-I$srcroot/include $AM_CPPFLAGS"
AC_SUBST(AM_CPPFLAGS)

#
# There is way too much Makefile for nothing...
#
AC_CONFIG_FILES([Makefile
		include/Makefile
		include/rattd/Makefile
		src/Makefile])
AC_OUTPUT
