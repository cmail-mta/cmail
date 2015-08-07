#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct ArgumentItem {

	char* argShort;
	char* argLong;
	void* func;
	unsigned arguments;
	struct ArgumentItem* next;

};

struct ArgumentItem* base;

int eargs_addArgument(char* argShort, char* argLong, void* func, unsigned arguments) {

	// init struct and fill with arguments
	struct ArgumentItem* item = (struct ArgumentItem*) malloc(sizeof(struct ArgumentItem));
	item->argShort = argShort;
	item->argLong = argLong;
	item->func = func;
	item->arguments = arguments;
	item->next = NULL;

	// check if base is initialized.
	if (!base) {
		base = item;
	} else {
		// find last item in argument list
		struct ArgumentItem* next = base;
		while (next->next) {
			next = next->next;
		}

		// add new argument
		next->next = item;
	}

	return 1;
}

int eargs_clearItem(struct ArgumentItem* item) {
	
	// free when last item else recursive
	if (item->next) {
		eargs_clearItem(item->next);
	} else {
		free(item);
	}

	return 1;
}

int eargs_clear() {

	// check if base is initialized.
	if (base) {
		// begin clearing
		if (base->next) {
			eargs_clearItem(base->next);
		}
		free(base);
	}

	return 1;
}

int eargs_parseItem(int argc, char** cmds) {

	struct ArgumentItem* item = base;
	int arg = -1;

	// no args
	if (argc < 1) {
		return arg;
	}

	while (item) {

		// check if argShort matches or if argLong matches (NULL will be excluded)
		if ((item->argShort && !strcmp(cmds[0], item->argShort)) || (item->argLong && !strcmp(cmds[0], item->argLong))) {
		
			// check if enough arguments are available
			if (argc > item->arguments) {
			
				
				// call function
				int (*p)(int argc, char** argv) = item->func;
				if (p(argc, cmds) < 0) {
					// error in function (bad input?)
					return -2;
				}
				arg = item->arguments;
			} else {
				printf("(%s,%s) needs an argument.", item->argShort,item->argLong);
				return -2;
			}
		}
		
		item = item->next;
	}

	return arg;
}

// output should be initialized with: argc * sizeof(char*))
int eargs_parse(int argc, char** argv, char** output) {

	// memset output array (don't trust);
	memset(output, 0, argc * sizeof(char*));
	int outputc = 0;
	int i;

	for (i = 1; i < argc; i++) {
		int v = eargs_parseItem(argc - i, &argv[i]);

		// -2 means error in parsing the argument
		if (v == -2) {
			return -2;
		// -1 means no identifier found for this argument -> add to output list
		} else if (v < 0) {
			output[outputc] = argv[i];
			outputc++;
		} else {
			// skip arguments used by identifier.
			i += v;
		}
	}

	// clear struct
	eargs_clear();

	return outputc;
}
