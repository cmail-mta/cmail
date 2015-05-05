#include <string.h>
#include <stdlib.h>
#define MAX_CFGLINE 2048
#define STATIC_SEND_BUFFER_LENGTH	1024

char* common_strdup(char* input){
	char* duplicate=NULL;
	size_t length=strlen(input);
	duplicate=calloc(length+1, sizeof(char));
	if(!duplicate){
		return NULL;
	}

	return strncpy(duplicate, input, length); //this is ok because the memory is calloc'd
}
