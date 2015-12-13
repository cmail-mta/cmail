export DOCDIR?=$(DESTDIR)$(PREFIX)/share/man
.SUFFIXES: .8 .8.gz
.PHONY: dist-doc install clean

.8.8.gz:
	gzip -c < $< > $@

dist-doc: cmail-smtpd.8.gz cmail-dispatchd.8.gz cmail-popd.8.gz

install: dist-doc
	install -g 0 -o 0 -m 0644 *.8.gz $(DOCDIR)/man8/

clean:
	$(RM) *.gz
