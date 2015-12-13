#include "../lib/common.h"
#include "../lib/logger.h"
#include "../lib/common.c"
#include "../lib/logger.c"

int main(int argc, char** argv){
	unsigned i, max;
	uint8_t randomness;
	if(argc < 2){
		return -1;
	}

	max = strtoul(argv[1], NULL, 10);

	for(i = 0; i < max; i++){
		common_rand(&randomness, 1);
		printf("%02X\n", randomness);
	}
}
