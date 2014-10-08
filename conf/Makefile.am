MAINTAINERCLEANFILES = Makefile.in

def-conf:
	test -d $(DESTDIR)$(sysconfdir) || $(mkinstalldirs) $(DESTDIR)$(sysconfdir)
	for conffile in `find $(top_srcdir)/conf/ -maxdepth 1 -name \*.xml -o -name \*.xsd` ; do \
		filename=`echo $$conffile | sed -e 's|^.*/||'`; \
		$(INSTALL) -m 644 $(top_srcdir)/conf/$$filename $(DESTDIR)$(sysconfdir); \
	done

	test -d $(DESTDIR)$(sysconfdir)/client-profiles || $(mkinstalldirs) $(DESTDIR)$(sysconfdir)/client-profiles
	for conffile in `find $(top_srcdir)/conf/client-profiles/ -maxdepth 1 -name \*.xml -o -name \*.xsd` ; do \
		filename=`echo $$conffile | sed -e 's|^.*/||'`; \
		$(INSTALL) -m 644 $(top_srcdir)/conf/client-profiles/$$filename $(DESTDIR)$(sysconfdir)/client-profiles; \
	done

install-data-local:
	test -d $(DESTDIR)$(sysconfdir) || $(MAKE) def-conf
