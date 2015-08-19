#include "easy_args.h"
#include <stdlib.h>

struct config {
	unsigned verbosity;
	char* dbpath;
};

int help(int argc, char* argv[], void* __attribute__((unused)) config) {
	// not used here
	usage(PROGRAM_NAME);

	return -1;
}

int set_verbosity(int argc, char* argv[], void* config) {
	struct config* c = (struct config*) config;
	c->verbosity = strtoul(argv[1], NULL, 10);

	return 0;
}

int set_dbpath(int argc, char* argv[], void* config) {
	struct config* c = (struct config*) config;
	c->dbpath = argv[1];

	return 0;
}

int add_args() {
	eargs_addArgument("-d", "--dbpath", set_dbpath, 1);
	eargs_addArgument("-h", "--help", help, 0);
	eargs_addArgument("-v", "--verbosity", set_verbosity, 1);

	return 0;
}
