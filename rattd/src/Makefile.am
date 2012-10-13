#
# src/Makefile.am
#

sbin_PROGRAMS = rattd
rattd_SOURCES = rattd.c conf.c main.c dtor.c log.c module.c table.c
rattd_LDFLAGS = @libconfig_LIBS@ @LIBADD_DLOPEN@ -rdynamic
