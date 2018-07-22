.PHONY: clean run
CFLAGS ?= -Wall -g
LDLIBS = -lsqlite3 -lgnutls -lnettle

LIB = $(wildcard ../lib/*.c) $(wildcard ../lib/*.h)
HEADERS = $(wildcard *.h)
SOURCES = smtpd.c $(filter-out smtpd.c,$(wildcard *.c))
LIBOBJ = ../lib/logger.o

all: cmail-smtpd

cmail-smtpd: $(SOURCES) $(HEADERS) $(LIB) $(LIBOBJ)
	$(CC) $(CFLAGS) $< $(LIBOBJ) $(LDLIBS) -o $@

run: all
	valgrind --leak-check=full ./cmail-smtpd devel-smtpd.conf nodetach

clean:
	$(RM) cmail-smtpd
	$(RM) $(LIBOBJ)
