MAINTAINERCLEANFILES 	   = Makefile.in

INCLUDES                   = -Iinclude \
                             -I$(top_srcdir)/libs/mrcp-signaling/include \
                             -I$(top_srcdir)/libs/mrcp/include \
                             -I$(top_srcdir)/libs/mrcp/message/include \
                             -I$(top_srcdir)/libs/mrcp/control/include \
                             -I$(top_srcdir)/libs/mrcp/resources/include \
                             -I$(top_srcdir)/libs/mpf/include \
                             -I$(top_srcdir)/libs/uni-rtsp/include \
                             -I$(top_srcdir)/libs/apr-toolkit/include \
                             $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES) $(UNIMRCP_SOFIA_INCLUDES)

noinst_LTLIBRARIES         = libmrcpunirtsp.la

include_HEADERS            = include/mrcp_unirtsp_server_agent.h \
libmrcpunirtsp_la_SOURCES  = src/mrcp_unirtsp_server_agent.c
libmrcpunirtsp_la_LIBADD   = libunirtsp.la $(UNIMRCP_SOFIA_LIBS)
