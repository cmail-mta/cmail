.PHONY: clean
CFLAGS=-Wall -g
LDLIBS=-lsqlite3 -lnettle

all: 
	cd user && make all
	cd address && make all

	mkdir -p bin
	mv user/cmail-admin-user bin/
	mv address/cmail-admin-address bin/

clean:
	rm cmail-admin-user
	rm cmail-admin-address
