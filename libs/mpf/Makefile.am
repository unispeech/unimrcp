MAINTAINERCLEANFILES 	 = Makefile.in

INCLUDES                 = -I$(top_srcdir)/libs/mpf/include \
                           -I$(top_srcdir)/libs/apr-toolkit/include \
                           $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_LTLIBRARIES       = libmpf.la

include_HEADERS          = include/mpf.h \
                           include/mpf_message.h \
                           include/mpf_engine.h \
                           include/mpf_timer.h \
                           include/mpf_context.h

libmpf_la_SOURCES        = src/mpf_engine.c \
                           src/mpf_timer.c \
                           src/mpf_context.c
