.PHONY: clean
CFLAGS=-Wall -g
LDLIBS=-lsqlite3 -lnettle -lgnutls -ladns

all: mta
	mv mta cmail-mta

popd: mta.c

run: all
	valgrind --leak-check=full ./cmail-mta mta.conf nodetach

clean:
	rm cmail-mta
