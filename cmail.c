#include "cmail.h"

int usage(char* filename){
	printf("%s usage information\n", VERSION);
	//TODO
	return EXIT_FAILURE;
}

void logprintf(LOGGER log, unsigned level, char* fmt, ...){
	va_list args;

	va_start(args, fmt);
	if(log.verbosity>=level){
		vfprintf(log.stream, fmt, args);
	}
	va_end(args);
}

int main(int argc, char** argv){
	ARGUMENTS args = {
		.config_file = NULL,
		.drop_privileges = true, 
		.detach = true
	};

	CONFIGURATION config = {
		.master = NULL,
		.listeners = {
			.count = 0,
			.fds = NULL
		},
		.privileges = {
			.uid=0,
			.gid=0
		},
		.log = {
			.stream = stderr,
			.verbosity = 0
		}
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

	logprintf(config.log, LOG_INFO, "This is %s, starting up\n", VERSION);

	//attach aux databases
	//TODO

	//drop privileges
	if(getuid() == 0 && args.drop_privileges){
		//TODO initgroups
		if(setgid(config.privileges.gid) != 0){
			perror("setgid");
			exit(EXIT_FAILURE);
		}
		if(setuid(config.privileges.uid) != 0){
			perror("setuid");
			exit(EXIT_FAILURE);
		}
		//TODO check for success
	}
	
	//detach from console (or dont)
	//TODO
	
	//enter main processing loop
	//TODO
	
	//clean up allocated resources
	logprintf(config.log, LOG_INFO, "Cleaning up resources\n");
	arguments_free(&args);
	config_free(&config);

	return EXIT_SUCCESS;
}
