#include "msa.h"

int usage(char* filename){
	printf("%s - Part of the cmail internet mail processing suite\n", VERSION);
	printf("Accept incoming mail\n");
	printf("Usage: %s <conffile> [options]\n", filename);
	printf("Recognized options:\n");
	printf("\tnodrop\t\tDo not drop privileges\n");
	printf("\tnodetach\tDo not detach from console\n");
	return EXIT_FAILURE;
}

int main(int argc, char** argv){
	ARGUMENTS args = {
		.config_file = NULL,
		.drop_privileges = true, 
		.detach = true
	};

	CONFIGURATION config = {
		.database = {
			.conn = NULL,
			.query_authdata = NULL,
			.query_addresses = NULL,
			.query_inrouter = NULL,
			.query_outrouter = NULL,
			.mail_storage = {
				.mailbox_master = NULL,
				.outbox_master = NULL,
				.users = NULL
			}
		},
		.listeners = {
			.count = 0,
			.conns = NULL
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

	#ifndef CMAIL_NO_TLS
	if(gnutls_global_init()){
		arguments_free(&args);
		fprintf(stderr, "Failed to initialize GnuTLS\n");
		exit(EXIT_FAILURE);
	}
	#endif
	
	//read config file
	if(config_parse(config.log, &config, args.config_file)<0){
		arguments_free(&args);
		config_free(&config);
		TLSSUPPORT(gnutls_global_deinit());
		exit(usage(argv[0]));
	}

	logprintf(config.log, LOG_INFO, "This is %s, starting up\n", VERSION);

	if(signal_init(config.log)<0){
		arguments_free(&args);
		config_free(&config);
		TLSSUPPORT(gnutls_global_deinit());
		exit(EXIT_FAILURE);
	}

	//attach aux databases
	if(database_initialize(config.log, &(config.database))<0){
		arguments_free(&args);
		config_free(&config);
		TLSSUPPORT(gnutls_global_deinit());
		exit(EXIT_FAILURE);
	}

	//drop privileges
	if(getuid() == 0 && args.drop_privileges){
		if(privileges_drop(config.log, config.privileges)<0){
			arguments_free(&args);
			config_free(&config);
			exit(EXIT_FAILURE);
		}
	}
	else{
		logprintf(config.log, LOG_INFO, "Not dropping privileges%s\n", (args.drop_privileges?" (Because you are not root)":""));
	}
	
	//detach from console (or dont)
	if(args.detach && config.log.stream!=stderr){
		logprintf(config.log, LOG_INFO, "Detaching from parent process\n");
		
		//flush the stream so we do not get everything twice
		fflush(config.log.stream);
		
		switch(daemonize(config.log)){
			case 0:
				break;
			case 1:
				logprintf(config.log, LOG_INFO, "Parent process going down\n");
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
		logprintf(config.log, LOG_INFO, "Not detaching from console%s\n", (args.detach?" (Because the log output stream is stderr)":""));
	}
	
	//enter main processing loop
	core_loop(config.log, config.listeners, &(config.database));
	
	//clean up allocated resources
	logprintf(config.log, LOG_INFO, "Cleaning up resources\n");
	arguments_free(&args);
	config_free(&config);
	TLSSUPPORT(gnutls_global_deinit());

	return EXIT_SUCCESS;
}
