.PHONY: clean
CFLAGS=-Wall -g
LDLIBS=-lsqlite3 -lnettle -lgnutls -ladns

all: mta
	mv mta cmail-mta

mta: mta.c

run: all
	valgrind --leak-check=full ./cmail-mta mta.conf nodetach

clean:
	$(RM) cmail-mta
