bin_PROGRAMS = expidus1-power-manager

expidus1_power_manager_SOURCES =                   \
	$(BUILT_SOURCES)			\
	espm-main.c				\
	espm-manager.c				\
	espm-manager.h				\
	espm-power.c				\
	espm-power.h				\
	espm-battery.c				\
	espm-battery.h				\
	espm-esconf.c				\
	espm-esconf.h				\
	espm-console-kit.c			\
	espm-console-kit.h			\
	espm-systemd.c				\
	espm-systemd.h				\
	egg-idletime.c				\
	egg-idletime.h				\
	espm-backlight.c			\
	espm-backlight.h			\
	espm-kbd-backlight.c			\
	espm-kbd-backlight.h			\
	espm-dpms.c				\
	espm-dpms.h				\
	espm-button.c				\
	espm-button.h				\
	espm-network-manager.c			\
	espm-network-manager.h			\
	espm-inhibit.c				\
	espm-inhibit.h				\
	espm-notify.c				\
	espm-notify.h				\
	espm-polkit.c				\
	espm-polkit.h				\
	espm-errors.c				\
	espm-errors.h				\
	espm-suspend.c				\
	espm-suspend.h				\
	expidus-screensaver.c			\
	expidus-screensaver.h			\
	../panel-plugins/power-manager-plugin/power-manager-button.c	\
	../panel-plugins/power-manager-plugin/power-manager-button.h	\
	../panel-plugins/power-manager-plugin/scalemenuitem.c			\
	../panel-plugins/power-manager-plugin/scalemenuitem.h

expidus1_power_manager_CFLAGS =                    \
	-I$(top_srcdir)                         \
	-I$(top_srcdir)/common                  \
	-I$(top_srcdir)/libdbus                 \
	-DLOCALEDIR=\"$(localedir)\"            \
	-DG_LOG_DOMAIN=\"expidus1-power-manager\"  \
	-DSYSCONFDIR=\"$(sysconfdir)\"          \
	-DSBINDIR=\"$(sbindir)\"                \
	-DUPOWER_ENABLE_DEPRECATED              \
	-DESPM_SUSPEND_HELPER_CMD=\"$(prefix)/bin/expidus1-pm-helper\" \
	-UEXPIDUS_PLUGIN                           \
	-DESPM_SYSTRAY							\
	$(GOBJECT_CFLAGS)                       \
	$(GTHREAD_CFLAGS)                       \
	$(DBUS_GLIB_CFLAGS)                     \
	$(LIBEXPIDUS1UI_CFLAGS)                    \
	$(ESCONF_CFLAGS)                        \
	$(LIBNOTIFY_CFLAGS)                     \
	$(UPOWER_CFLAGS)                        \
	$(XRANDR_CFLAGS)			\
	$(DPMS_CFLAGS)           		\
	$(PLATFORM_CPPFLAGS)			\
	$(PLATFORM_CFLAGS)

expidus1_power_manager_LDFLAGS =			\
	-no-undefined				\
	$(PLATFORM_LDFLAGS)

expidus1_power_manager_LDADD =                     \
	$(top_builddir)/common/libespmcommon.la \
	$(top_builddir)/libdbus/libespmdbus.la  \
	$(GOBJECT_LIBS)                         \
	$(GTHREAD_LIBS)                         \
	$(DBUS_GLIB_LIBS)                       \
	$(LIBEXPIDUS1UI_LIBS)                      \
	$(ESCONF_LIBS)                          \
	$(LIBNOTIFY_LIBS)                       \
	$(UPOWER_LIBS)                          \
	$(XRANDR_LIBS)				\
	$(DPMS_LIBS)

if ENABLE_POLKIT

sbin_PROGRAMS = espm-power-backlight-helper     \
	   expidus1-pm-helper

espm_power_backlight_helper_SOURCES =           \
       espm-backlight-helper.c

espm_power_backlight_helper_LDADD =             \
       $(GLIB_LIBS)                             \
       -lm

espm_power_backlight_helper_CFLAGS =            \
        $(GLIB_CFLAGS)                          \
	$(PLATFORM_CPPFLAGS)			\
	$(PLATFORM_CFLAGS)

expidus1_pm_helper_SOURCES =  \
	espm-pm-helper.c

expidus1_pm_helper_CFLAGS =   \
	-I$(top_srcdir)        \
	$(GLIB_CFLAGS)         \
	$(PLATFORM_CPPFLAGS)   \
	$(PLATFORM_CFLAGS)     \
	$(LIBEXPIDUS1UTIL_CFLAGS)

expidus1_pm_helper_LDFLAGS = \
	$(LIBEXPIDUS1UTIL_LIBS)  \
	$(GLIB_LIBS)

polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_DATA =					\
	com.expidus.power.policy

# You will need a recent intltool or the patch from this bug
# http://bugzilla.gnome.org/show_bug.cgi?id=462312
@INTLTOOL_POLICY_RULE@

.in2.in:
	sed "s|[@]sbindir@|${sbindir}|" $< > $@

endif

manpagedir = $(mandir)/man1

manpage_DATA = expidus1-power-manager.1

BUILT_SOURCES =					\
	expidus-power-manager-dbus.c		\
	expidus-power-manager-dbus.h		\
	espm-marshal.c                          \
        espm-marshal.h				\
	org.freedesktop.PowerManagement.c       \
	org.freedesktop.PowerManagement.h       \
	org.freedesktop.PowerManagement.Inhibit.c \
	org.freedesktop.PowerManagement.Inhibit.h

if MAINTAINER_MODE

espm-marshal.c: espm-marshal.list
	echo "#include \"espm-marshal.h\"" > $@ && \
	glib-genmarshal $< --prefix=_espm_marshal --body >> $@

espm-marshal.h: espm-marshal.list
	glib-genmarshal $< --prefix=_espm_marshal --header > $@

expidus-power-manager-dbus.c expidus-power-manager-dbus.h: $(srcdir)/com.expidus.Power.Manager.xml
	gdbus-codegen --interface-prefix=com.expidus. --c-namespace=Espm --generate-c-code=expidus-power-manager-dbus $<

org.freedesktop.PowerManagement.c org.freedesktop.PowerManagement.h: $(srcdir)/org.freedesktop.PowerManagement.xml
	gdbus-codegen --interface-prefix=org.freedesktop. --c-namespace=Espm --generate-c-code=org.freedesktop.PowerManagement $<

org.freedesktop.PowerManagement.Inhibit.c org.freedesktop.PowerManagement.Inhibit.h: $(srcdir)/org.freedesktop.PowerManagement.Inhibit.xml
	gdbus-codegen --interface-prefix=org.freedesktop. --c-namespace=Espm --generate-c-code=org.freedesktop.PowerManagement.Inhibit $<

endif

@INTLTOOL_DESKTOP_RULE@
autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = expidus1-power-manager.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

EXTRA_DIST =                                    \
	espm-marshal.list			\
        $(autostart_in_files)                   \
        com.expidus.power.policy.in2              \
        $(manpage_DATA)

DISTCLEANFILES =                                \
        expidus1-power-manager.desktop             \
        com.expidus.power.policy                  \
        com.expidus.power.policy.in

MAINTAINERCLEANFILES =                                \
        $(BUILT_SOURCES)
