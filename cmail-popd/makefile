.PHONY: clean
CFLAGS=-Wall -g
LDLIBS=-lsqlite3 -lnettle

all: popd
	mv popd cmail-popd

popd: popd.c

clean:
	rm cmail-popd
