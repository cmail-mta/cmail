#include "cmail.h"

int usage(char* filename){
	printf("%s usage information\n", VERSION);
	//TODO
	return EXIT_FAILURE;
}

int main(int argc, char** argv){
	ARGUMENTS args = {
		.config_file = NULL,
		.drop_privileges = true, 
		.detach = true
	};

	//parse arguments
	if(!arguments_parse(&args, argc-1, argv+1)){
		arguments_free(&args);
		exit(usage(argv[1]));
	}
	
	//read config file
	//TODO

	fprintf(stderr, "This is %s, starting up\n", VERSION);

	//open & attach databases
	//TODO

	//bind all ports
	//TODO

	//drop privileges
	//TODO
	
	//detach from console (or dont)
	//TODO
	
	//enter main processing loop
	//TODO
	
	//clean up allocated resources
	//TODO

	fprintf(stderr, "Bye.");
	return EXIT_SUCCESS;
}
