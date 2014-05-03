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
  videoob.h videoob.c \
  weboobresources.h weboobresources.c
#  grl-weboob.c grl-weboob.h 

extdir				= $(GRL_PLUGINS_DIR)
weboobxmldir		= $(GRL_PLUGINS_DIR)
weboobxml_DATA	= $(WEBOOB_PLUGIN_ID).xml

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
