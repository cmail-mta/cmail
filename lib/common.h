#include <string.h>
#define MAX_CFGLINE 2048

char* common_strdup(char* input){
	char* duplicate=NULL;
	size_t length=strlen(input);
	duplicate=calloc(length+1, sizeof(char));
	if(!duplicate){
		return NULL;
	}
	
	return strncpy(duplicate, input, length);
}
