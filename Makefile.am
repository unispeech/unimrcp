macrodir		= @ac_macro_dir@
auxdir			= @ac_aux_dir@
AUX_DIST		= $(auxdir)/config.guess \
                          $(auxdir)/config.sub \
                          $(auxdir)/install-sh \
                          $(auxdir)/ltconfig \
                          $(auxdir)/ltmain.sh \
                          $(auxdir)/depcomp \
                          $(auxdir)/missing
EXTRA_DIST		= bootstrap

AUTOMAKE_OPTIONS 	= foreign
MAINTAINERCLEANFILES 	= Makefile.in aclocal.m4 configure $(AUX_DIST)

ACLOCAL			= aclocal -I $(macrodir)

SUBDIRS                 = libs modules plugins platforms
if TEST_SUITES
SUBDIRS                += tests
endif

dox:
	doxygen $(top_srcdir)/docs/doxygen.conf

def-conf:
	test -d $(confdir) || $(mkinstalldirs) $(confdir)
	for conffile in `find conf -name \*.xml` ; do \
	    filename=`echo $$conffile | sed -e 's|^.*/||'`; \
	    $(INSTALL) -m 644 conf/$$filename $(confdir); \
	done
           
install-data-local:
	test -d $(confdir) || $(MAKE) def-conf
