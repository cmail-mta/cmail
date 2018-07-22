.PHONY: clean all
CFLAGS ?= -Wall -g
LDLIBS = -lsqlite3 -lnettle
LIBOBJ = ../../lib/logger.o

all: cmail-admin-user

cmail-admin-user: cmail-admin-user.c ../lib/easy_args.c $(LIBOBJ)

clean:
	$(RM) cmail-admin-user
	$(RM) $(LIBOBJ)
