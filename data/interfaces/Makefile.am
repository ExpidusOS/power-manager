interfaces =					\
	espm-settings.ui

interfaces_h =					\
	$(interfaces:.ui=_ui.h)

if MAINTAINER_MODE

BUILT_SOURCES = $(interfaces_h)

espm-settings_ui.h: espm-settings.ui
	xdt-csource --static --strip-comments --strip-content --name=espm_settings_ui $< > $@

endif

CLEANFILES =					\
	$(BUILT_SOURCES)

EXTRA_DIST =					\
	$(interfaces)				\
	$(interfaces_h)
