bin_PROGRAMS = expidus1-power-manager-settings

expidus1_power_manager_settings_SOURCES = 				\
	expidus-power-manager-dbus.h			\
	expidus-power-manager-dbus.c			\
	espm-settings-main.c					\
	espm-settings-app.c					\
	espm-settings-app.h					\
	espm-settings.c						\
	espm-settings.h						\
	$(top_srcdir)/common/espm-config.h				\
	$(top_srcdir)/common/espm-enum.h				\
	$(top_srcdir)/common/espm-enum-glib.h

expidus1_power_manager_settings_CFLAGS =				\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/common					\
	-I$(top_srcdir)/src					\
	-I$(top_srcdir)/data					\
	-I$(top_srcdir)/libdbus                 		\
	-DLOCALEDIR=\"$(localedir)\"				\
	-DG_LOG_DOMAIN=\"expidus1-power-manager-settings\" 	\
	-export-dynamic						\
	$(MODULE_FLAGS)						\
	$(GTK_CFLAGS)						\
	$(GLIB_CFLAGS)						\
	$(GTHREAD_CFLAGS)					\
	$(GMODULE_CFLAGS)					\
	$(DBUS_CFLAGS)						\
	$(DBUS_GLIB_CFLAGS)					\
	$(LIBEXPIDUS1UI_CFLAGS)					\
	$(LIBEXPIDUS1UTIL_CFLAGS)					\
	$(ESCONF_CFLAGS)					\
	$(UPOWER_CFLAGS)

expidus1_power_manager_settings_LDADD   =				\
	$(top_builddir)/libdbus/libespmdbus.la  		\
	$(top_builddir)/common/libespmcommon.la			\
	$(GTK_LIBS)						\
	$(GLIB_LIBS)						\
	$(GTHREAD_LIBS)						\
	$(GMODULE_LIBS)						\
	$(DBUS_LIBS)						\
	$(DBUS_GLIB_LIBS)					\
	$(LIBEXPIDUS1UI_LIBS)					\
	$(LIBEXPIDUS1UTIL_LIBS)					\
	$(ESCONF_LIBS)						\
	$(UPOWER_LIBS)


manpagedir = $(mandir)/man1

manpage_DATA = expidus1-power-manager-settings.1

if MAINTAINER_MODE

BUILT_SOURCES = 			\
	expidus-power-manager-dbus.c	\
	expidus-power-manager-dbus.h

expidus-power-manager-dbus.c expidus-power-manager-dbus.h: $(top_srcdir)/src/com.expidus.Power.Manager.xml
	gdbus-codegen --interface-prefix=com.expidus. --c-namespace=Espm --generate-c-code=expidus-power-manager-dbus $<

endif

#
# .desktop file
#
desktop_in_files = expidus1-power-manager-settings.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA =	$(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = 					\
	$(desktop_in_files)			\
	$(manpage_DATA)

DISTCLEANFILES =				\
	$(BUILT_SOURCES)			\
	$(desktop_DATA)
