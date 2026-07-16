ACLOCAL_AMFLAGS = -I m4

# FIXME: make docs work with distcheck
DISTCHECK_CONFIGURE_FLAGS = --disable-docs

EXTRA_DIST = \
	imquic.pc.in \
	tools/l4s/analyze_timeseries.py \
	tools/l4s/run_timeseries_test.sh
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = imquic.pc
DISTCLEANFILES = imquic.pc

SUBDIRS = src examples docs
#~ dist_html_DATA = README.md

.PHONY: FORCE
FORCE:

.PHONY: l4s-timeseries-check
l4s-timeseries-check:
	$(top_srcdir)/tools/l4s/run_timeseries_test.sh
