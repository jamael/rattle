#
# Makefile.am
#

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = $(want_tests) src modules

if WANT_TESTS
want_tests = tests
else
want_tests =
endif

rattle_includedir = $(includedir)/rattle
rattle_include_HEADERS = \
	include/rattle/conf.h \
	include/rattle/def.h \
	include/rattle/log.h \
	include/rattle/module.h \
	include/rattle/proc.h \
	include/rattle/table.h
