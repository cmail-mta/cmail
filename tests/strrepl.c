#include <stdio.h>
#include "../lib/logger.h"
#include "../lib/common.h"
#include "../lib/common.c"
#include "../lib/logger.c"

int main(int argc, char** argv){
	char data[20] = "asdfooasd";

	if(common_strrepl(data, sizeof(data), argv[1], argv[2]) < 0){
		printf("Failed\n");
		return -1;
	}

	printf("Result: %s\n", data);
	return 0;
}

