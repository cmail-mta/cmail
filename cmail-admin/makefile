.PHONY: clean
CFLAGS=-Wall -g
LDLIBS=-lsqlite3 -lnettle

all:
	cd user && make all
	cd address && make all
	cd msa && make all
	cd permissions && make all
	cd popd && make all

	mkdir -p bin
	mv user/cmail-admin-user bin/
	mv address/cmail-admin-address bin/
	mv msa/cmail-admin-msa bin/
	mv permissions/cmail-admin-permissions bin/
	mv popd/cmail-admin-popd bin/
	cp cmail-admin bin/

clean:
	rm bin/*
