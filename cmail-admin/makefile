.PHONY: clean
CFLAGS=-Wall -g
LDLIBS=-lsqlite3 -lnettle

all: 
	cd user && make all
	cd address && make all
	cd msa && make all
	cd permissions && make all

	mkdir -p bin
	mv user/cmail-admin-user bin/
	mv address/cmail-admin-address bin/
	mv msa/cmail-admin-msa bin/
	mv permissions/cmail-admin-permissions bin/

clean:
	rm bin/*
