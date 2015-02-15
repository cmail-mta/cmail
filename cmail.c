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

int daemonize(LOGGER log){
	int pid = fork();
	if(pid < 0){
		logprintf(log, LOG_ERROR, "Failed to fork\n");
		return -1;
	}

	if(pid == 0){
		close(0);
		close(1);
		close(2);
		setsid();
		return 0;
	}

	else{
		return 1;
	}
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
	if(database_initialize(config.log, config.master)<0){
		arguments_free(&args);
		config_free(&config);
		exit(EXIT_FAILURE);
	}

	//drop privileges
	if(getuid() == 0 && args.drop_privileges){
		logprintf(config.log, LOG_INFO, "Dropping privileges...\n");
		//TODO initgroups
		//TODO exit with cleanup here
		if(chdir("/")<0){
			logprintf(config.log, LOG_ERROR, "Failed to drop privileges (changing directories): %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		if(setgid(config.privileges.gid) != 0){
			logprintf(config.log, LOG_ERROR, "Failed to drop privileges (changing gid): %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		if(setuid(config.privileges.uid) != 0){
			logprintf(config.log, LOG_ERROR, "Failed to drop privileges (changing uid): %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		//TODO check for success
	}
	else{
		logprintf(config.log, LOG_INFO, "Not dropping privileges\n");
	}
	
	//detach from console (or dont)
	if(args.detach && config.log.stream!=stderr){
		logprintf(config.log, LOG_INFO, "Detaching from parent process\n");
		switch(daemonize(config.log)){
			case 0:
				break;
			case 1:
				arguments_free(&args);
				config_free(&config);
				exit(EXIT_SUCCESS);
				break;
			case -1:
				arguments_free(&args);
				config_free(&config);
				exit(EXIT_FAILURE);
		}
	}
	else{
		logprintf(config.log, LOG_INFO, "Not detaching from console\n");
	}
	
	//enter main processing loop
	//TODO
	logprintf(config.log, LOG_INFO, "MAIN THREAD STUFF\n");
	
	//clean up allocated resources
	logprintf(config.log, LOG_INFO, "Cleaning up resources\n");
	arguments_free(&args);
	config_free(&config);

	return EXIT_SUCCESS;
}
