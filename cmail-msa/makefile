.PHONY: clean
CFLAGS=-Wall -g
LDLIBS=-lsqlite3 -lgnutls -lnettle

all: cmail
	mv cmail cmail-msa

cmail: cmail.c

clean:
	rm cmail-msa
