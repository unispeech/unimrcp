MAINTAINERCLEANFILES 	 = Makefile.in

INCLUDES                 = -I$(top_srcdir)/libs/mrcp/include \
                           -I$(top_srcdir)/libs/mrcp/parser/include \
                           -I$(top_srcdir)/libs/apr-toolkit/include \
                           $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_LTLIBRARIES       = libmrcp.la

include_HEADERS          = include/mrcp.h \
                           parser/include/mrcp_header_accessor.h \
                           parser/include/mrcp_generic_header.h \
                           parser/include/mrcp_message.h \
                           parser/include/mrcp_resource.h \
                           parser/include/mrcp_parser.h

libmrcp_la_SOURCES       = parser/src/mrcp_header_accessor.c \
                           parser/src/mrcp_generic_header.c \
                           parser/src/mrcp_message.c \
                           parser/src/mrcp_resource.c \
                           parser/src/mrcp_parser.c
