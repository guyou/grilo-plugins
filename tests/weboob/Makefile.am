#
# Makefile.am
#
# Author: Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
#
# Copyright (C) 2014 Guilhem Bonnefille

include $(top_srcdir)/gtester.mk

INCLUDES = @DEPS_CFLAGS@

TEST_PROGS +=	\
   test_videoob \
   test_weboob

test_videoob_defines =                                              \
   -DWEBOOB_PLUGIN_PATH=\""$(abs_top_builddir)/src/weboob/.libs/"\" \
   -DWEBOOB_DATA_PATH=\""$(abs_top_srcdir)/tests/weboob/data/"\"

test_videoob_SOURCES =	\
	test_videoob.c

test_videoob_LDADD =	\
	@DEPS_LIBS@ $(abs_top_builddir)/src/weboob/.libs/libgrlweboob.la

test_videoob_CFLAGS =	\
	$(test_videoob_defines)

test_weboob_defines =                                              \
   -DWEBOOB_PLUGIN_PATH=\""$(abs_top_builddir)/src/weboob/.libs/"\" \
   -DWEBOOB_DATA_PATH=\""$(abs_top_srcdir)/tests/weboob/data/"\"

test_weboob_SOURCES =	\
	test_weboob.c

test_weboob_LDADD =	\
	@DEPS_LIBS@

test_weboob_CFLAGS =	\
	$(test_weboob_defines)

# Distribute the tests data:
dist_noinst_DATA =  

noinst_PROGRAMS = $(TEST_PROGS)

MAINTAINERCLEANFILES =  \
   *.in                 \
   *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
