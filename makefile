export PREFIX?=/usr/sbin
export MKDIR?=mkdir -p
LOGDIR?=$(DESTDIR)/var/log/cmail
CONFDIR?=$(DESTDIR)/etc/cmail
DBDIR?=$(DESTDIR)$(CONFDIR)/databases
.PHONY: clean install init tls-init rtldumps

all:
	@$(MAKE) -C cmail-smtpd
	@$(MAKE) -C cmail-dispatchd
	@$(MAKE) -C cmail-popd
	@$(MAKE) -C cmail-imapd
	@$(MAKE) -C cmail-admin

	$(MKDIR) bin
	@mv cmail-smtpd/cmail-smtpd bin/
	@mv cmail-dispatchd/cmail-dispatchd bin/
	@mv cmail-popd/cmail-popd bin/
	# mv cmail-imapd/cmail-imapd bin/

install:
	@printf "Installing to %s%s\n" "$(DESTDIR)" "$(PREFIX)"
	install -m 0755 bin/* "$(DESTDIR)$(PREFIX)"
	$(MAKE) -C cmail-admin install

uninstall:
	@printf "Removing daemon binaries from %s%s\n" "$(DESTDIR)" "$(PREFIX)"
	$(RM) $(DESTDIR)$(PREFIX)/cmail-smtpd
	$(RM) $(DESTDIR)$(PREFIX)/cmail-dispatchd
	$(RM) $(DESTDIR)$(PREFIX)/cmail-popd
	#remove old binaries
	$(RM) $(DESTDIR)$(PREFIX)/cmail-msa
	$(RM) $(DESTDIR)$(PREFIX)/cmail-mta
	$(MAKE) -C cmail-admin uninstall

init:
	@printf "*** Testing for sqlite3 CLI binary\n"
	@sqlite3 --version
	@printf "\n*** Creating cmail user\n"
	-adduser --disabled-login cmail
	@printf "\n*** Creating configuration directories\n"
	$(MKDIR) "$(LOGDIR)"
	$(MKDIR) "$(CONFDIR)"
	$(MKDIR) "$(DBDIR)"
	chown root:cmail "$(DBDIR)"
	chmod 770 "$(DBDIR)"
	@printf "\n*** Copying example configuration files to %s\n" "$(CONFDIR)"
	cp example-configs/*.conf.src "$(CONFDIR)"
	@printf "\n*** Updating the sample configuration files with dynamic defaults\n"
	sed -e 's,LOGFILE,$(LOGDIR)/cmail-smtpd.log,' -e 's,MASTERDB,$(DBDIR)/master.db3,' "$(CONFDIR)/smtpd.conf.src" > $(CONFDIR)/smtpd.conf 
	sed -e 's,LOGFILE,$(LOGDIR)/cmail-dispatchd.log,' -e 's,MASTERDB,$(DBDIR)/master.db3,' "$(CONFDIR)/dispatchd.conf.src" > $(CONFDIR)/dispatchd.conf 
	sed -e 's,LOGFILE,$(LOGDIR)/cmail-popd.log,' -e 's,MASTERDB,$(DBDIR)/master.db3,' "$(CONFDIR)/popd.conf.src" > $(CONFDIR)/popd.conf 
	@printf "\n*** Creating empty master database in %s/master.db3\n" "$(DBDIR)"
	cat sql-update/install_master.sql | sqlite3 "$(DBDIR)/master.db3"
	chown root:cmail "$(DBDIR)/master.db3"
	chmod 770 "$(DBDIR)/master.db3"

tls-init: init
	@printf "\n*** Creating certificate storage directory in %s/keys\n" "$(CONFDIR)"
	$(MKDIR) "$(CONFDIR)/keys"
	chmod 700 "$(CONFDIR)/keys"
	@printf "\n*** Creating temporary TLS certificate in %s/keys\n" "$(CONFDIR)"
	openssl req -x509 -newkey rsa:8192 -keyout "$(CONFDIR)/keys/temp.key" -out "$(CONFDIR)/keys/temp.cert" -days 100 -nodes
	chmod 600 "$(CONFDIR)/keys"/*
	@printf "\n*** Updating the configuration files in %s\n" "$(CONFDIR)"
	sed -i -e 's,TLSCERT,$(CONFDIR)/keys/temp.cert,' -e 's,TLSKEY,$(CONFDIR)/keys/temp.key,' -e 's,#cert,cert,g' "$(CONFDIR)/smtpd.conf"
	sed -i -e 's,TLSCERT,$(CONFDIR)/keys/temp.cert,' -e 's,TLSKEY,$(CONFDIR)/keys/temp.key,' -e 's,#cert,cert,g' "$(CONFDIR)/popd.conf"

uninit:
	@printf "\n*** Removing databases from %s\n" "$(DBDIR)"
	$(RM) -r $(DBDIR)
	@printf "\n*** Removing logfiles from %s\n" "$(LOGDIR)"
	$(RM) -r $(LOGDIR)
	@printf "\n*** Removing configuration data from %s\n" "$(CONFDIR)"
	$(RM) -r $(CONFDIR)
	@printf "\n*** Removing cmail user\n"
	userdel cmail

rtldumps:
	@-rm -rf rtldumps
	$(MAKE) CC=gcc CFLAGS=-fdump-rtl-expand -C cmail-smtpd
	$(MAKE) CC=gcc CFLAGS=-fdump-rtl-expand -C cmail-dispatchd
	$(MAKE) CC=gcc CFLAGS=-fdump-rtl-expand -C cmail-popd
	$(MKDIR) rtldumps
	mv cmail-smtpd/*.expand rtldumps/
	mv cmail-dispatchd/*.expand rtldumps/
	mv cmail-popd/*.expand rtldumps/

cppcheck:
	@printf "Running cppcheck on cmail-smtpd\n"
	cppcheck --enable=all cmail-smtpd/smtpd.c
	@printf "\nRunning cppcheck on cmail-dispatchd\n"
	cppcheck --enable=all cmail-dispatchd/dispatchd.c
	@printf "\nRunning cppcheck on cmail-popd\n"
	cppcheck --enable=all cmail-popd/popd.c

clean:
	$(RM) bin/*
	@$(MAKE) -C cmail-smtpd clean
	@$(MAKE) -C cmail-dispatchd clean
	@$(MAKE) -C cmail-popd clean
	@$(MAKE) -C cmail-imapd clean
	@$(MAKE) -C cmail-admin clean

cleanup-bins:
	$(RM) $(which cmail-mta)
	$(RM) $(which cmail-msa)
