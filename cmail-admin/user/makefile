.PHONY: clean all
CFLAGS=-Wall -g
LDLIBS=-lsqlite3 -lnettle

all: cmail-admin-user

cmail-admin-user: cmail-admin-user.c ../lib/easy_args.c

clean:
	rm cmail-admin-user
