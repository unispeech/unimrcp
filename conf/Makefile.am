MAINTAINERCLEANFILES = Makefile.in

def-conf:
	test -d $(DESTDIR)$(confdir) || $(mkinstalldirs) $(DESTDIR)$(confdir)
	for conffile in `find ./ -maxdepth 1 -name \*.xml -o -name \*.xsd` ; do \
		filename=`echo $$conffile | sed -e 's|^.*/||'`; \
		$(INSTALL) -m 644 $$filename $(DESTDIR)$(confdir); \
	done

	test -d $(DESTDIR)$(confdir)/client-profiles || $(mkinstalldirs) $(DESTDIR)$(confdir)/client-profiles
	for conffile in `find ./client-profiles/ -maxdepth 1 -name \*.xml -o -name \*.xsd` ; do \
		filename=`echo $$conffile | sed -e 's|^.*/||'`; \
		$(INSTALL) -m 644 client-profiles/$$filename $(DESTDIR)$(confdir)/client-profiles; \
	done


install-data-local:
	test -d $(DESTDIR)$(confdir) || $(MAKE) def-conf
