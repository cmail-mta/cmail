.PHONY: clean run
CFLAGS ?= -Wall -g
LDLIBS = -lsqlite3 -lnettle -lgnutls -ladns

LIB = $(wildcard ../lib/*.c) $(wildcard ../lib/*.h)
HEADERS = $(wildcard *.h)
SOURCES = dispatchd.c $(filter-out dispatchd.c,$(wildcard *.c))
LIBOBJ = ../lib/logger.o

all: cmail-dispatchd

cmail-dispatchd: $(SOURCES) $(HEADERS) $(LIB) $(LIBOBJ)
	$(CC) $(CFLAGS) $< $(LIBOBJ) $(LDLIBS) -o $@

run: all
	valgrind --leak-check=full ./cmail-dispatchd devel-dispatchd.conf nodetach

clean:
	$(RM) cmail-dispatchd
	$(RM) $(LIBOBJ)
