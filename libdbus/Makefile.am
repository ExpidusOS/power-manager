noinst_LTLIBRARIES = 			\
	libespmdbus.la

libespmdbus_la_SOURCES =		\
	espm-dbus.c			\
	espm-dbus.h			\
	espm-dbus-monitor.c		\
	espm-dbus-monitor.h		\
	espm-dbus-marshal.c		\
	espm-dbus-marshal.h
	
libespmdbus_la_CFLAGS =			\
	$(GLIB_CFLAGS)			\
	$(LIBEXPIDUS1UTIL_CFLAGS)		\
	$(DBUS_GLIB_CFLAGS)

libespmdbus_la_LIBADD =			\
	$(GLIB_LIBS)			\
	$(DBUS_GLIB_LIBS)		\
	$(DBUS_LIBS)

if MAINTAINER_MODE

BUILT_SOURCES =				\
	espm-dbus-marshal.c		\
	espm-dbus-marshal.h

espm-dbus-marshal.c: espm-dbus-marshal.list
	echo "#include \"espm-dbus-marshal.h\"" > $@ && \
	glib-genmarshal $< --prefix=_espm_dbus_marshal --body >> $@

espm-dbus-marshal.h: espm-dbus-marshal.list
	glib-genmarshal $< --prefix=_espm_dbus_marshal --header > $@

endif

EXTRA_DIST =				\
	espm-dbus-marshal.list

DISTCLEANFILES =			\
	$(BUILT_SOURCES)
