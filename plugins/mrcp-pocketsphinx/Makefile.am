INCLUDES                    = $(UNIMRCP_PLUGIN_INCLUDES) \
                              $(UNIMRCP_SPHINXBASE_INCLUDES) \
                              $(UNIMRCP_POCKETSPHINX_INCLUDES)

plugin_LTLIBRARIES          = mrcppocketsphinx.la

mrcppocketsphinx_la_SOURCES = src/mrcp_pocketsphinx.c \
                              src/pocketsphinx_properties.c
mrcppocketsphinx_la_LDFLAGS = $(UNIMRCP_PLUGIN_OPTS)
mrcppocketsphinx_la_LIBADD  = $(UNIMRCP_POCKETSPHINX_LIBS) $(UNIMRCP_SPHINXBASE_LIBS) -lm

dictionary:
	!(test -f $(UNIMRCP_POCKETSPHINX_MODELS)/lm/cmudict.0.6d) || \
		$(INSTALL) -m 644 $(UNIMRCP_POCKETSPHINX_MODELS)/lm/cmudict.0.6d $(DESTDIR)$(datadir)/default.dic

model:
	!(test -d $(UNIMRCP_POCKETSPHINX_MODELS)/hmm/wsj1/) || ($(mkinstalldirs) $(DESTDIR)$(datadir)/wsj1; \
		$(INSTALL) -m 644 $(UNIMRCP_POCKETSPHINX_MODELS)/hmm/wsj1/* $(DESTDIR)$(datadir)/wsj1)

install-data-local:
	test -d $(DESTDIR)$(confdir) || $(mkinstalldirs) $(DESTDIR)$(confdir)
	test -f $(DESTDIR)$(confdir)/pocketsphinx.xml || $(INSTALL) -m 644 conf/pocketsphinx.xml $(DESTDIR)$(confdir)
	test -f $(DESTDIR)$(datadir)/default.dic || $(MAKE) dictionary
	test -d $(DESTDIR)$(datadir)/wsj1 || $(MAKE) model

include $(top_srcdir)/build/rules/uniplugin.am
