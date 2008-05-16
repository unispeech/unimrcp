MAINTAINERCLEANFILES 	 = Makefile.in

INCLUDES                 = -I$(top_srcdir)/libs/mrcp/include \
                           -I$(top_srcdir)/libs/mrcp/message/include \
                           -I$(top_srcdir)/libs/mrcp/control/include \
                           -I$(top_srcdir)/libs/apr-toolkit/include \
                           $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_LTLIBRARIES       = libmrcp.la

include_HEADERS          = include/mrcp.h \
                           include/mrcp_types.h \
                           parser/include/mrcp_header_accessor.h \
                           parser/include/mrcp_generic_header.h \
                           parser/include/mrcp_message.h \
                           control/include/mrcp_control_channel.h \
                           control/include/mrcp_resource.h \
                           control/include/mrcp_resource_factory.h

libmrcp_la_SOURCES       = parser/src/mrcp_header_accessor.c \
                           parser/src/mrcp_generic_header.c \
                           parser/src/mrcp_message.c \
                           control/src/mrcp_resource_factory.c
