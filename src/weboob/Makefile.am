#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES = libgrlweboob.la

libgrlweboob_la_CFLAGS =	\
	$(DEPS_WEBOOB_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlWeboob\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlweboob_la_LIBADD =	\
	$(DEPS_WEBOOB_LIBS)

libgrlweboob_la_LDFLAGS = \
	-no-undefined      \
	-module		   \
	-avoid-version

libgrlweboob_la_SOURCES = \
  grl-weboob-shared.h \
  grl-weboob.c grl-weboob.h \
  videoob.h videoob.c \
  weboob.h weboob.c \
  weboobresources.h weboobresources.c

extdir				= $(GRL_PLUGINS_DIR)
weboobxmldir		= $(GRL_PLUGINS_DIR)
weboobxml_DATA	= $(WEBOOB_PLUGIN_ID).xml

# This lets us test the plugin without installing it,
# because grilo expects the .so and .xml files to be in
# the same directory:
copy-xml-to-libs-dir: ${ext_LTLIBRARIES}
	cp -f $(srcdir)/$(weboobxml_DATA) $(builddir)/.libs/

all-local: copy-xml-to-libs-dir

weboobresources.h: weboob.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/weboob.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_weboob --generate-header
weboobresources.c: weboob.gresource.xml weboobresources.h weboob.png
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/weboob.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_weboob --generate-source

EXTRA_DIST = $(weboobxml_DATA) weboob.png weboob.gresource.xml

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
