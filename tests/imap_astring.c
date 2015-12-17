#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/common.h"
#include "../lib/logger.h"
#include "../lib/common.c"
#include "../lib/logger.c"
#include "../cmail-imapd/protocol.c"

int main(int argc, char** argv){
	char* input_string = NULL;
	ssize_t result = 0;
	char* string_begin = NULL;
	char* string_end = NULL;
	char* data_end = NULL;

	if(argc == 2){
		//TODO read n bytes
	}
	else if(argc > 2 && !strcmp(argv[1], "test")){
		input_string = strdup(argv[2]);
	}
	else{
		printf("Usage: %s [<bytes to read> | test <string>]\n", argv[0]);
		return 1;
	}

	printf("Input string: '%s'\n", input_string);
	result = protocol_parse_astring(input_string, &string_begin, &string_end, &data_end);
	
	printf("Output string: '%s'\n", input_string);
	printf("Resulting bytes: %d\n", result);
	if(result >= 0){
		if(!string_begin || !string_end || !data_end){
			printf("At least one output pointer invalid\n");
		}
		else{
			printf("string_begin @ %d: '%s'\n", string_begin - input_string, string_begin);
			printf("string_end @ %d: '%s'\n", string_end - input_string, string_end);
			printf("data_end @ %d: '%s'\n", data_end - input_string, data_end);
		}
	}

	free(input_string);
	return 1;
}
