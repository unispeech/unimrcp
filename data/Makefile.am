MAINTAINERCLEANFILES = Makefile.in

def-data:
	test -d $(DESTDIR)$(datadir) || $(mkinstalldirs) $(DESTDIR)$(datadir)
	for datafile in `find ./ -name \*.pcm -o -name \*.xml -o -name \*.jsgf -o -name \*.txt` ; do \
		filename=`echo $$datafile | sed -e 's|^.*/||'`; \
		$(INSTALL) -m 644 $$filename $(DESTDIR)$(datadir); \
	done

install-data-local:
	test -d $(DESTDIR)$(datadir) || $(MAKE) def-data
