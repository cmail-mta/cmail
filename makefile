PREFIX?=/usr/sbin
.PHONY: clean install

all:
	@$(MAKE) -C cmail-msa
	@$(MAKE) -C cmail-mta
	@$(MAKE) -C cmail-popd
	@$(MAKE) -C cmail-imapd

	@-mkdir -p bin
	@mv cmail-msa/cmail-msa bin/
	@mv cmail-mta/cmail-mta bin/
	@mv cmail-popd/cmail-popd bin/
	# mv cmail-imapd/cmail-imapd bin/

install:
	@printf "Installing to %s\n" "$(PREFIX)"
	install -m 0755 bin/cmail-msa $(PREFIX)
	install -m 0755 bin/cmail-mta $(PREFIX)
	install -m 0755 bin/cmail-popd $(PREFIX)
	#install -m 0755 bin/cmail-imapd $(PREFIX)

clean:
	rm bin/*


