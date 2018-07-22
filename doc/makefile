PREFIX ?= /usr
export DOCDIR ?= $(DESTDIR)$(PREFIX)/share/man
.PHONY: dist-doc install clean
MANPAGES = $(patsubst %.8,%.8.gz,$(notdir $(wildcard *.8)))

%.8.gz :: %.8
	gzip -c < $< > $@

dist-doc: ${MANPAGES}

install: dist-doc
	install -g 0 -o 0 -m 0644 ${MANPAGES} $(DOCDIR)/man8/

clean:
	$(RM) ${MANPAGES}
