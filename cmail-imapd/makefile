.PHONY: clean run
CFLAGS=-Wall -g
LDLIBS=-lsqlite3 -lgnutls -lnettle -lpthread

all: imapd
	mv imapd cmail-imapd

imapd: imapd.c

run: all
	valgrind --leak-check=full ./cmail-imapd devel-imapd.conf nodetach

clean:
	$(RM) cmail-imapd
