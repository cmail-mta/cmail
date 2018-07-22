.PHONY: clean
CFLAGS ?= -Wall -g
LDLIBS = -lsqlite3
LIBOBJ = ../../lib/logger.o

all: cmail-admin-smtpd

cmail-admin-smtpd: cmail-admin-smtpd.c ../lib/easy_args.c $(LIBOBJ)

clean:
	$(RM) cmail-admin-smtpd
	$(RM) $(LIBOBJ)
