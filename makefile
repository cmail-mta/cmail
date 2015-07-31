PREFIX?=/usr/local/sbin
LOGDIR?=/var/log/cmail
CONFDIR?=/etc/cmail
DBDIR?=$(CONFDIR)/databases
.PHONY: clean install init tls-init rtldumps

all:
	@$(MAKE) -C cmail-msa
	@$(MAKE) -C cmail-mta
	@$(MAKE) -C cmail-popd
	@$(MAKE) -C cmail-imapd
	@$(MAKE) -C cmail-admin

	@-mkdir -p bin
	@mv cmail-msa/cmail-msa bin/
	@mv cmail-mta/cmail-mta bin/
	@mv cmail-popd/cmail-popd bin/
	# mv cmail-imapd/cmail-imapd bin/

install:
	@printf "Installing to %s\n" "$(PREFIX)"
	install -m 0755 bin/cmail-msa "$(PREFIX)"
	install -m 0755 bin/cmail-mta "$(PREFIX)"
	install -m 0755 bin/cmail-popd "$(PREFIX)"
	#install -m 0755 bin/cmail-imapd "$(PREFIX)"

	$(MAKE) -C cmail-admin install

init:
	@printf "*** Testing for sqlite3 CLI binary\n"
	@sqlite3 --version
	@printf "\n*** Creating cmail user\n"
	-adduser --disabled-login cmail
	@printf "\n*** Creating configuration directories\n"
	mkdir -p "$(LOGDIR)"
	mkdir -p "$(CONFDIR)"
	mkdir -p "$(DBDIR)"
	chown root:cmail "$(DBDIR)"
	chmod 770 "$(DBDIR)"
	@printf "\n*** Copying example configuration files to %s\n" "$(CONFDIR)"
	cp example-configs/*.conf.src "$(CONFDIR)"
	@printf "\n*** Updating the sample configuration files with dynamic defaults\n"
	sed -e 's,LOGFILE,$(LOGDIR)/msa.log,' -e 's,MASTERDB,$(DBDIR)/master.db3,' "$(CONFDIR)/msa.conf.src" > $(CONFDIR)/msa.conf 
	@printf "\n*** Creating empty master database in %s/master.db3\n" "$(DBDIR)"
	cat sql-update/install_master.sql | sqlite3 "$(DBDIR)/master.db3"
	chown root:cmail "$(DBDIR)/master.db3"
	chmod 770 "$(DBDIR)/master.db3"
	# This target needs sqlite3 installed, TODO: mention this somewhere

tls-init: init
	@printf "\n*** Creating certificate storage directory in %s/keys\n" "$(CONFDIR)"
	mkdir -p "$(CONFDIR)/keys"
	chmod 700 "$(CONFDIR)/keys"
	@printf "\n*** Creating temporary TLS certificate in %s/keys\n" "$(CONFDIR)"
	openssl req -x509 -newkey rsa:8192 -keyout "$(CONFDIR)/keys/temp.key" -out "$(CONFDIR)/keys/temp.cert" -days 100 -nodes
	chmod 600 "$(CONFDIR)/keys"/*
	@printf "\n*** Remember to add the generated certificates/keys to the configuration in %s" "$(CONFDIR)"
	@printf "\n*** eg. bind * 25 cert=%s key=%s ciphers=NORMAL:%%LATEST_RECORD_VERSION:-VERS-SSL3.0" "$(CONFDIR)/keys/temp.cert" "$(CONFDIR)/keys/temp.key"

rtldumps:
	@-rm -rf rtldumps
	$(MAKE) CC=gcc CFLAGS=-fdump-rtl-expand -C cmail-msa
	$(MAKE) CC=gcc CFLAGS=-fdump-rtl-expand -C cmail-mta
	$(MAKE) CC=gcc CFLAGS=-fdump-rtl-expand -C cmail-popd
	@-mkdir -p rtldumps
	mv cmail-msa/*.expand rtldumps/
	mv cmail-mta/*.expand rtldumps/
	mv cmail-popd/*.expand rtldumps/

cppcheck:
	@printf "Running cppcheck on cmail-msa\n"
	cppcheck --enable=all cmail-msa/msa.c
	@printf "\nRunning cppcheck on cmail-mta\n"
	cppcheck --enable=all cmail-mta/mta.c
	@printf "\nRunning cppcheck on cmail-popd\n"
	cppcheck --enable=all cmail-popd/popd.c

clean:
	rm bin/*
