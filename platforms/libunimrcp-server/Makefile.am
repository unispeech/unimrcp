MAINTAINERCLEANFILES 	    = Makefile.in

INCLUDES                    = -I$(top_srcdir)/platforms/libunimrcp-server/include \
                              -I$(top_srcdir)/libs/mrcp-server/include \
                              -I$(top_srcdir)/libs/mrcp-signaling/include \
                              -I$(top_srcdir)/libs/mrcp/include \
                              -I$(top_srcdir)/libs/mrcp/message/include \
                              -I$(top_srcdir)/libs/mrcp/control/include \
                              -I$(top_srcdir)/libs/mrcp/resources/include \
                              -I$(top_srcdir)/libs/mpf/include \
                              -I$(top_srcdir)/libs/apr-toolkit/include \
                              $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

lib_LTLIBRARIES             = libunimrcpserver.la

include_HEADERS             = include/unimrcp_server.h

libunimrcpserver_la_SOURCES = src/unimrcp_server.c

libunimrcpserver_la_LIBADD  = $(top_builddir)/libs/mrcp-server/libmrcpserver.la \
                              $(top_builddir)/libs/mrcp-signaling/libmrcpsignaling.la \
                              $(top_builddir)/libs/mrcp/libmrcp.la \
                              $(top_builddir)/libs/mpf/libmpf.la \
                              $(top_builddir)/libs/apr-toolkit/libaprtoolkit.la \
                              $(UNIMRCP_APR_LIBS) $(UNIMRCP_APU_LIBS)
