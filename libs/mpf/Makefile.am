MAINTAINERCLEANFILES 	 = Makefile.in

INCLUDES                 = -I$(top_srcdir)/libs/mpf/include \
                           -I$(top_srcdir)/libs/apr-toolkit/include \
                           $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_LTLIBRARIES       = libmpf.la

include_HEADERS          = include/mpf.h \
                           include/mpf_audio_file_stream.h \
                           include/mpf_bridge.h \
                           include/mpf_codec.h \
                           include/mpf_codec_descriptor.h \
                           include/mpf_context.h \
                           include/mpf_engine.h \
                           include/mpf_frame.h \
                           include/mpf_message.h \
                           include/mpf_object.h \
                           include/mpf_stream.h \
                           include/mpf_timer.h

libmpf_la_SOURCES        = src/mpf_audio_file_stream.c \
                           src/mpf_bridge.c \
                           src/mpf_context.c \
                           src/mpf_engine.c \
                           src/mpf_timer.c
