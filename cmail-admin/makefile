.PHONY: clean
MKDIR?=mkdir -p
CFLAGS=-Wall -g
LDLIBS=-lsqlite3 -lnettle

all:
	$(MAKE) -C user
	$(MAKE) -C address
	$(MAKE) -C smtpd
	$(MAKE) -C permissions
	$(MAKE) -C popd

	$(MKDIR) bin
	mv user/cmail-admin-user bin/
	mv address/cmail-admin-address bin/
	mv smtpd/cmail-admin-smtpd bin/
	mv permissions/cmail-admin-permissions bin/
	mv popd/cmail-admin-popd bin/
	cp cmail-admin bin/

install:
	install -m 0755 bin/* "$(DESTDIR)$(PREFIX)"

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/cmail-admin*

clean:
	$(RM) bin/*
