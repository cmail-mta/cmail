.PHONY: all clean run
CFLAGS ?= -Wall -g
LDLIBS = -lsqlite3 -lgnutls -lnettle -lpthread

LIB = $(wildcard ../lib/*.c) $(wildcard ../lib/*.h)
HEADERS = $(wildcard *.h)
SOURCES = imapd.c $(filter-out imapd.c,$(wildcard *.c))
LIBOBJ = ../lib/logger.o

all: cmail-imapd

cmail-imapd: $(SOURCES) $(HEADERS) $(LIB) $(LIBOBJ)
	$(CC) $(CFLAGS) $< $(LIBOBJ) $(LDLIBS) -o $@

run: all
	valgrind --leak-check=full ./cmail-imapd devel-imapd.conf nodetach

helgrind: all
	valgrind --tool=helgrind ./cmail-imapd devel-imapd.conf nodetach

clean:
	$(RM) cmail-imapd
	$(RM) $(LIBOBJ)
