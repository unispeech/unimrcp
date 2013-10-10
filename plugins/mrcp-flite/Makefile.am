INCLUDES                   = $(UNIMRCP_PLUGIN_INCLUDES) \
                             $(UNIMRCP_FLITE_INCLUDES)

plugin_LTLIBRARIES         = mrcpflite.la

mrcpflite_la_SOURCES       = src/mrcp_flite.c \
                             src/flite_voices.c
mrcpflite_la_LDFLAGS       = $(UNIMRCP_PLUGIN_OPTS)
mrcpflite_la_LIBADD        = $(UNIMRCP_FLITE_LIBS) -lm

include $(top_srcdir)/build/rules/uniplugin.am
