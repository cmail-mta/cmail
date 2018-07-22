.PHONY: clean
CFLAGS ?= -Wall -g
LDLIBS = -lsqlite3
LIBOBJ = ../../lib/logger.o

all: cmail-admin-permissions

cmail-admin-permissions: cmail-admin-permissions.c ../lib/easy_args.c $(LIBOBJ)

clean:
	$(RM) cmail-admin-permissions
	$(RM) $(LIBOBJ)
