#
# Makefile.am
#

SUBDIRS = modules include

ACLOCAL_AMFLAGS = -I m4
DEFAULT_INCLUDES = -I. -Iinclude

sbin_PROGRAMS = rattd
rattd_SOURCES =		\
	src/rattd.c	\
	src/conf.c	\
	src/main.c	\
	src/dtor.c	\
	src/log.c	\
	src/module.c	\
	src/table.c	\
	src/proc.c	\
	src/signal.c	\
	src/args.c	\
	src/debug.c

rattd_LDFLAGS = @libconfig_LIBS@ @LIBADD_DLOPEN@ -rdynamic
