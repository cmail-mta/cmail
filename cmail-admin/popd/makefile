.PHONY: clean all
CFLAGS ?= -Wall -g
LDLIBS = -lsqlite3
LIBOBJ = ../../lib/logger.o

all: cmail-admin-popd

cmail-admin-popd: cmail-admin-popd.c ../lib/easy_args.c $(LIBOBJ)

clean:
	$(RM) cmail-admin-popd
	$(RM) $(LIBOBJ)
