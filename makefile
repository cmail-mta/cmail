INSTALL_PATH=/usr/local/bin
.PHONY: clean install

all:
	@$(MAKE) -C cmail-msa
	@$(MAKE) -C cmail-mta
	@$(MAKE) -C cmail-popd
	@$(MAKE) -C cmail-imapd

	@-mkdir bin
	@mv cmail-msa/cmail-msa bin/
	@mv cmail-mta/cmail-mta bin/
	@mv cmail-popd/cmail-popd bin/
	# mv cmail-imapd/cmail-imapd bin/

install:
	install -m 0755 bin/cmail-msa $(INSTALL_PATH)
	install -m 0755 bin/cmail-mta $(INSTALL_PATH)
	install -m 0755 bin/cmail-popd $(INSTALL_PATH)
	# install -m 0755 bin/cmail-imapd $(INSTALL_PATH)

clean:
	rm bin/*


