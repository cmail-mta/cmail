export DOCDIR?=$(DESTDIR)/usr/share/man
.SUFFIXES: .8 .8.gz
.PHONY: dist-doc install clean

.8.8.gz:
	gzip -c < $< > $@

dist-doc: cmail-admin-address.8.gz cmail-admin-popd.8.gz cmail-admin-smtpd.8.gz cmail-admin-user.8.gz

install: dist-doc
	install -g 0 -o 0 -m 0644 *.8.gz $(DOCDIR)/man8/

clean:
	$(RM) *.gz
