.PHONY: clean run
CFLAGS=-Wall -g -DCMAIL_TEST_REPL
LDLIBS=-lsqlite3 -lgnutls -lnettle

all: smtpd
	mv smtpd cmail-smtpd

smtpd: smtpd.c

run: all
	valgrind --leak-check=full ./cmail-smtpd devel-smtpd.conf nodetach

clean:
	$(RM) cmail-smtpd
