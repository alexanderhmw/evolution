module_LTLIBRARIES = module-bogofilter.la

module_bogofilter_la_CPPFLAGS = \
	$(AM_CPPFLAGS)						\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"evolution-bogofilter\"			\
	-DWELCOME_MESSAGE=\""$(privdatadir)/default/C/mail/local/Inbox"\" \
	$(EVOLUTION_DATA_SERVER_CFLAGS)				\
	$(GNOME_PLATFORM_CFLAGS)				\
	$(CODE_COVERAGE_CFLAGS)

module_bogofilter_la_SOURCES = \
	evolution-bogofilter.c

module_bogofilter_la_LIBADD = \
	$(top_builddir)/e-util/libevolution-util.la		\
	$(top_builddir)/mail/libevolution-mail.la		\
	$(top_builddir)/libemail-engine/libemail-engine.la	\
	$(EVOLUTION_DATA_SERVER_LIBS)				\
	$(GNOME_PLATFORM_LIBS)

module_bogofilter_la_LDFLAGS = \
	-module -avoid-version $(NO_UNDEFINED) $(CODE_COVERAGE_LDFLAGS)

@INTLTOOL_XML_RULE@
metainfodir = $(datadir)/appdata
metainfo_in_files = evolution-bogofilter.metainfo.xml.in
metainfo_files = $(metainfo_in_files:.xml.in=.xml)
dist_metainfo_DATA = $(metainfo_files)

EXTRA_DIST = $(metainfo_in_files)

DISTCLEANFILES = $(metainfo_files)

-include $(top_srcdir)/git.mk
