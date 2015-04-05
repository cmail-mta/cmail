.PHONY: clean run
CFLAGS=-Wall -g
LDLIBS=-lsqlite3 -lgnutls -lnettle

all: msa
	mv msa cmail-msa

msa: msa.c

run: all
	valgrind --leak-check=full ./cmail-msa msa.conf nodetach

clean:
	rm cmail-msa
