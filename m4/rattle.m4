dnl RATTLE_MODULE(module_name)
dnl
dnl Add a --without-module-name to configure and WANT_MODULE_NAME
dnl for Automake conditional build.
dnl
AC_DEFUN([RATTLE_MODULE],[
	AC_ARG_WITH(translit($1,A-Z_,a-z-),
		[AS_HELP_STRING([--without-]translit($1,A-Z_,a-z-),
			[without ]$1[ rattle module])],
			[[with_]$1[="${withval}"]], [with_]$1[="yes"]
	)
	AM_CONDITIONAL([WANT_]translit($1,a-z,A-Z), [test x$with_$1 = xyes])
])
