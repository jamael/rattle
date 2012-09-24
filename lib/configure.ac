AC_INIT([librattle], [0.1])

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

# Absolute root path of SRCDIR
srcroot=$(cd $srcdir; pwd)

# Search headers locally first
AM_CPPFLAGS="-I$srcroot/include $AM_CPPFLAGS"
AC_SUBST(AM_CPPFLAGS)

AC_CONFIG_FILES([Makefile
		src/Makefile
		include/Makefile
		include/rattle/Makefile])
AC_OUTPUT
