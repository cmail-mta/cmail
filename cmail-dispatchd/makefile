.PHONY: clean
CFLAGS=-Wall -g
LDLIBS=-lsqlite3 -lnettle -lgnutls -ladns

all: dispatchd
	mv dispatchd cmail-dispatchd

dispatchd: dispatchd.c

run: all
	valgrind --leak-check=full ./cmail-dispatchd devel-dispatchd.conf nodetach

clean:
	$(RM) cmail-dispatchd
