.PHONY: clean all
CFLAGS?=-Wall -g
LDLIBS=-lsqlite3

all: cmail-admin-popd

cmail-admin-popd: cmail-admin-popd.c ../lib/easy_args.c

clean:
	$(RM) cmail-admin-popd
