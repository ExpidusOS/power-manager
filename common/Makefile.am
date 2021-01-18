noinst_LTLIBRARIES =        \
        libespmcommon.la

libespmcommon_la_SOURCES =  \
	$(BUILT_SOURCES)        \
	espm-common.c           \
	espm-common.h           \
	espm-brightness.c       \
	espm-brightness.h       \
	espm-debug.c            \
	espm-debug.h            \
	espm-icons.h            \
	espm-power-common.c     \
	espm-power-common.h     \
	espm-enum.h             \
	espm-enum-glib.h

libespmcommon_la_CFLAGS =           \
	-I$(top_srcdir)                 \
	-DSBINDIR=\"$(sbindir)\"        \
	$(GTK_CFLAGS)                   \
	$(GLIB_CFLAGS)                  \
	$(LIBEXPIDUS1UTIL_CFLAGS)          \
	$(DBUS_GLIB_CFLAGS)             \
	$(UPOWER_CFLAGS)

libespmcommon_la_LIBADD =           \
	-lm                             \
	$(GTK_LIBS)                     \
	$(GLIB_LIBS)                    \
	$(XRANDR_LIBS)                  \
	$(LIBX11_LIBS)                     \
	$(DBUS_GLIB_LIBS)               \
	$(UPOWER_LIBS)

espm_glib_headers =                \
        $(srcdir)/espm-enum-glib.h

BUILT_SOURCES =       \
	espm-enum-types.c \
	espm-enum-types.h

if MAINTAINER_MODE

espm-enum-types.h: $(espm_glib_headers)
	( cd $(srcdir) && glib-mkenums \
		--fhead "#ifndef _ESPM_ENUM_TYPES_H\n#define _ESPM_ENUM_TYPES_H\n#include <glib-object.h>\nG_BEGIN_DECLS\n" \
		--fprod "/* enumerations from \"@filename@\" */\n" \
		--vhead "GType @enum_name@_get_type (void);\n#define ESPM_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
		--ftail "G_END_DECLS\n\n#endif /* _ESPM_ENUM_TYPES_H__ */" \
		$(espm_glib_headers) ) >> xgen \
	&& (cmp -s xgen espm-enum-types.h || cp xgen espm-enum-types.h ) \
	&& rm -f xgen xgen~

espm-enum-types.c: espm-enum-types.h
	( cd $(srcdir) && glib-mkenums \
		--fhead "#include <espm-enum-types.h>\n" \
		--fhead "#include \"espm-enum-glib.h\"\n\n" \
		--fprod "\n/* enumerations from \"@filename@\" */\n" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n\tstatic GType type = 0;\n\tif (type == 0) {\n\tstatic const G@Type@Value values[] = {"\
		--vprod "\t{ @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "\t{ 0, NULL, NULL }\n\t};\n\ttype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n\treturn type;\n}\n" \
		$(espm_glib_headers) ) > xgen \
	&& cp xgen espm-enum-types.c  \
	&& rm -f xgen xgen~

endif
