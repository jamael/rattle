#
# src/Makefile.am
#

ACLOCAL_AMFLAGS = -I m4

#
# Automake defines DEFAULT_INCLUDES to look into -I. for header files.
# However, that prevents picking up the desired system header if a local
# header of the same name exists in the source.
#
# #include <foo.h> and #include "foo.h" exists for that specific purpose.
#
DEFAULT_INCLUDES = -I.. -I../include

sbin_PROGRAMS = rattd
rattd_SOURCES = rattd.c conf.c main.c dtor.c log.c module.c table.c proc.c \
                signal.c
rattd_LDFLAGS = @libconfig_LIBS@ @LIBADD_DLOPEN@ -rdynamic

if WANT_TESTS
rattd_SOURCES += test.c
rattd_LDFLAGS += -Wl,--whole-archive,../tests/libtests.a,--no-whole-archive
endif
