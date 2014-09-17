MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS          = $(UNIMRCP_APR_INCLUDES)

noinst_PROGRAMS      = svnrev
svnrev_LDADD         = $(UNIMRCP_APR_LIBS)
svnrev_SOURCES       = svnrev.c

rev:
	./svnrev -rsvnrev.input -p../../ -o../uni_revision.h
