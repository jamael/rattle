AC_INIT([rattd], [0.1])

m4_include([m4/rattle.m4])
AC_DEFUN([RATTLE_MODULE_SRCDIR], [$srcroot/modules/]translit($1,A-Z_,a-z/))

AC_CONFIG_SRCDIR([LICENSE])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_FILES([Makefile
		include/Makefile
		modules/Makefile
		src/Makefile])

AM_INIT_AUTOMAKE([foreign subdir-objects])

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
PKG_CHECK_MODULES([libconfig], [libconfig >= 1.3],,
  AC_MSG_ERROR([libconfig 1.3 or newer not found.])
)
AC_SUBST(libconfig_CFLAGS)
AC_SUBST(libconfig_LIBS)

# absolute path to SRCDIR
srcroot=$(cd $srcdir; pwd)
AM_CPPFLAGS="-I$srcroot/include $AM_CPPFLAGS"
AC_SUBST(AM_CPPFLAGS)
