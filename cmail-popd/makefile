.PHONY: clean run
CFLAGS ?= -Wall -g
LDLIBS = -lsqlite3 -lnettle -lgnutls

LIB = $(wildcard ../lib/*.c) $(wildcard ../lib/*.h)
HEADERS = $(wildcard *.h)
SOURCES = popd.c $(filter-out popd.c,$(wildcard *.c))
LIBOBJ = ../lib/logger.o

all: cmail-popd

cmail-popd: $(SOURCES) $(HEADERS) $(LIB) $(LIBOBJ)
	$(CC) $(CFLAGS) $< $(LIBOBJ) $(LDLIBS) -o $@

run: all
	valgrind --leak-check=full ./cmail-popd devel-popd.conf nodetach

clean:
	$(RM) cmail-popd
	$(RM) $(LIBOBJ)
