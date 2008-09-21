MAINTAINERCLEANFILES 	 = Makefile.in

INCLUDES                 = -I$(top_srcdir)/libs/uni-rtsp/include \
                           -I$(top_srcdir)/libs/apr-toolkit/include \
                           $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_LTLIBRARIES       = libunirtsp.la

include_HEADERS          = include/rtsp.h \
                           include/rtsp_header.h \
                           include/rtsp_start_line.h \
                           include/rtsp_message.h \
                           include/rtsp_client_connection.h \
                           include/rtsp_server_connection.h

libunirtsp_la_SOURCES    = src/rtsp_header.c \
                           src/rtsp_start_line.c \
                           src/rtsp_message.c \
                           src/rtsp_client_connection \
                           src/rtsp_server_connection
