#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>

#include "../lib/common.h"
#include "../lib/logger.h"
#include "../lib/auth.h"

#include "../lib/common.c"
#include "../lib/auth.c"
#include "../lib/logger.c"

int main(int argc, char** argv){
	if(argc < 2){
		printf("Supply args\n");
		return EXIT_FAILURE;
	}

	LOGGER log = {
		.stream = stderr,
		.verbosity = LOG_DEBUG,
		.log_secondary = false,
		.print_timestamp = false
	};

	uint8_t* input = (uint8_t*)common_strdup(argv[1]);
	int status;

	printf("Input: %s\n", input);

	status = auth_base64encode(log, &input, strlen(input));

	printf("Encode (%d): %s\n", status, input);

	status = auth_base64decode(log, input);

	printf("Decode (%d): %s\n", status, input);

	return 0;
}
