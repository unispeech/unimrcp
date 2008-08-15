MAINTAINERCLEANFILES 	   = Makefile.in

INCLUDES                   = -Iinclude \
                             -I$(top_srcdir)/libs/mrcp-engine/include \
                             -I$(top_srcdir)/libs/mrcp/include \
                             -I$(top_srcdir)/libs/mrcp/message/include \
                             -I$(top_srcdir)/libs/mrcp/control/include \
                             -I$(top_srcdir)/libs/mrcp/resources/include \
                             -I$(top_srcdir)/libs/mpf/include \
                             -I$(top_srcdir)/libs/apr-toolkit/include \
                             $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

lib_LTLIBRARIES            = libdemosynth.la

libdemosynth_la_SOURCES    = src/demo_synth_engine.c
