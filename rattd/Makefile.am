#
# Makefile.am
#

SUBDIRS = modules include

ACLOCAL_AMFLAGS = -I m4

#
# Automake defines DEFAULT_INCLUDES to look into -I. for header files.
# However, that prevents picking up the desired system header if a local
# header of the same name exists in the source.
#
# #include <foo.h> and #include "foo.h" exists for that specific purpose.
#
DEFAULT_INCLUDES = -I. -Iinclude

sbin_PROGRAMS = rattd
rattd_SOURCES = \
	src/rattd.c \
	src/conf.c \
	src/main.c \
	src/dtor.c \
	src/log.c \
	src/module.c \
	src/table.c \
	src/proc.c \
	src/signal.c \
	src/test.c

rattd_LDFLAGS = @libconfig_LIBS@ @LIBADD_DLOPEN@ -rdynamic

#if WANT_TEST
#rattd_SOURCES += src/test.c
#rattd_LDFLAGS += -Wl,--whole-archive,../tests/libtests.a,--no-whole-archive
#endif
