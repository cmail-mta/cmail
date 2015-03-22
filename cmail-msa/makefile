.PHONY: clean run
CFLAGS=-Wall -g
LDLIBS=-lsqlite3 -lgnutls -lnettle

all: cmail
	mv cmail cmail-msa

cmail: cmail.c

run: all
	valgrind --leak-check=full ./cmail-msa msa.conf nodetach

clean:
	rm cmail-msa
