AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"expidus1-power-manager-plugin\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(PLATFORM_CPPFLAGS)

#
# expidus1 battery plugin
#
plugin_LTLIBRARIES = \
	libexpidus1powermanager.la

plugindir = \
	$(libdir)/expidus1/panel/plugins

libexpidus1powermanager_la_SOURCES = \
	expidus-power-manager-plugin.c         \
	../power-manager-button.c      \
	../power-manager-button.h      \
	../scalemenuitem.c       \
	../scalemenuitem.h

libexpidus1powermanager_la_CFLAGS =         \
	-I$(top_srcdir)	                \
	-I$(top_srcdir)/src             \
	-DLOCALEDIR=\"$(localedir)\"    \
	-DEXPIDUS_PLUGIN                   \
	$(LIBEXPIDUS1PANEL_CFLAGS)         \
	$(LIBEXPIDUS1UTIL_CFLAGS)          \
	$(LIBEXPIDUS1UI_CFLAGS)            \
	$(ESCONF_CFLAGS)                \
	$(GTK_CFLAGS)                   \
	$(GLIB_CFLAGS)                  \
	$(PLATFORM_CPPFLAGS)            \
	$(PLATFORM_CFLAGS)              \
	$(DBUS_GLIB_CFLAGS)             \
	$(UPOWER_CFLAGS)

libexpidus1powermanager_la_LDFLAGS = \
       -avoid-version \
       -module \
       -no-undefined \
       -export-symbols-regex '^expidus_panel_module_(preinit|init|construct)' \
       $(PLATFORM_LDFLAGS)

libexpidus1powermanager_la_LIBADD =                     \
	$(top_builddir)/common/libespmcommon.la \
	$(LIBEXPIDUS1PANEL_LIBS)                   \
	$(LIBEXPIDUS1UI_LIBS)                      \
	$(LIBEXPIDUS1UTIL_LIBS)                    \
	$(ESCONF_LIBS)                          \
	$(GTK_LIBS)                             \
	$(GLIB_LIBS)                            \
	$(DBUS_GLIB_LIBS)                       \
	$(UPOWER_LIBS)

#
# Desktop file
#
desktopdir = $(datadir)/expidus1/panel/plugins
desktop_in_in_files = power-manager-plugin.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =                                                            \
        $(desktop_in_in_files)

CLEANFILES =                                                            \
        $(desktop_in_files)                                             \
        $(desktop_DATA)

%.desktop.in: %.desktop.in.in
	sed -e "s,\@libdir\@,$(libdir),g" < $< > $@
