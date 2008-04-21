MAINTAINERCLEANFILES 	 = Makefile.in

INCLUDES                 = -I$(top_srcdir)/libs/apr-toolkit/include \
                           $(UNIMRCP_APR_INCLUDES) $(UNIMRCP_APU_INCLUDES)

noinst_LTLIBRARIES       = libaprtoolkit.la

include_HEADERS          = include/apt.h \
                           include/apt_obj_list.h
                           include/apt_task.h \
                           include/apt_composite_task.h \
                           include/apt_task_msg.h \

libaprtoolkit_la_SOURCES = src/apt_obj_list.c \
                           src/apt_task.c \
                           src/apt_composite_task.c
