.PHONY: clean
CFLAGS ?= -Wall -g
LDLIBS = -lsqlite3
LIBOBJ = ../../lib/logger.o

all: cmail-admin-address

cmail-admin-address: cmail-admin-address.c ../lib/easy_args.c $(LIBOBJ)

clean:
	$(RM) cmail-admin-address
	$(RM) $(LIBOBJ)
