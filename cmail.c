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

	CONFIGURATION config = {
		.listeners {
			.count = 0,
			.fds = NULL
		},
		.privileges = {
			.uid=0,
			.gid=0
		},
		.master = NULL,
		.logfd = -1
	};

	//parse arguments
	if(!arguments_parse(&args, argc-1, argv+1)){
		arguments_free(&args);
		exit(usage(argv[0]));
	}
	
	//read config file
	if(config_parse(&config, args.config_file)<0){
		arguments_free(&args);
		config_free(&config);
		exit(usage(argv[0]));
	}

	fprintf(stderr, "This is %s, starting up\n", VERSION);

	//attach aux databases
	//TODO

	//drop privileges
	if(getuid() == 0 && args.drop_privileges){
		if(setgid(config.privileges.gid) != 0){
			perror("setgid");
			exit(EXIT_FAILURE);
		}
		if(setuid(config.privileges.uid) != 0){
			perror("setuid");
			exit(EXIT_FAILURE);
		}
	}
	
	//detach from console (or dont)
	//TODO
	
	//enter main processing loop
	//TODO
	
	//clean up allocated resources
	arguments_free(&args);
	config_free(&config);

	fprintf(stderr, "Bye.");
	return EXIT_SUCCESS;
}
