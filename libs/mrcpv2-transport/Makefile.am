MAINTAINERCLEANFILES 	      = Makefile.in

INCLUDES                      = -I$(top_srcdir)/libs/mrcpv2-transport/include \
                                -I$(top_srcdir)/libs/mrcp/include \
                                -I$(top_srcdir)/libs/apr-toolkit/include \
                                $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_LTLIBRARIES            = libmrcpv2transport.la

include_HEADERS               = include/mrcpv2_connection_descriptor.h \
                                include/mrcpv2_client_agent.h \
                                include/mrcpv2_server_agent.h

libmrcpv2transport_la_SOURCES = src/mrcpv2_connection_descriptor.c \
                                src/mrcpv2_client_agent.c \
                                src/mrcpv2_server_agent.c
