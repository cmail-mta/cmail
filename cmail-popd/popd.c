#include "popd.h"

int usage(char* filename){
	printf("%s - Part of the cmail internet mail processing suite\n", VERSION);
	printf("Provide POP3 access to mailboxes\n");
	printf("Usage: %s <conffile> [options]\n", filename);
	printf("Recognized options:\n");
	printf("\tnodrop\t\tDo not drop privileges\n");
	printf("\tnodetach\tDo not detach from console\n");
	return EXIT_FAILURE;
}

int main(int argc, char** argv){
	FILE* pid_file = NULL;
	ARGUMENTS args = {
		.drop_privileges = true,
		.daemonize = true,
		.config_file = NULL
	};

	CONFIGURATION config = {
		.listeners = {
			.count = 0,
			.conns = NULL
		},
		.database = {
			.conn = NULL,
			.query_authdata = NULL,
			.query_userdatabase = NULL,
			.update_lock = NULL,
			.list_master = NULL,
			.fetch_master = NULL,
			.mark_deletion = NULL,
			.unmark_deletions = NULL,
			.delete_master = NULL
		},
		.privileges = {
			.uid = 0,
			.gid= 0
		},
		.pid_file = NULL
	};

	if(log_start()){
		return 1;
	}

	if(argc < 2){
		return usage(argv[0]);
	}

	//parse arguments
	if(!args_parse(&args, argc - 1, argv + 1)){
		return usage(argv[0]);
	}

	#ifndef CMAIL_NO_TLS
	if(gnutls_global_init()){
		fprintf(stderr, "Failed to initialize GnuTLS\n");
		exit(EXIT_FAILURE);
	}
	#endif

	//read config file
	if(config_parse(&config, args.config_file) < 0){
		config_free(&config);
		TLSSUPPORT(gnutls_global_deinit());
		return EXIT_FAILURE;
	}

	//initialize database
	if(database_initialize(&(config.database)) < 0){
		config_free(&config);
		TLSSUPPORT(gnutls_global_deinit());
		return EXIT_FAILURE;
	}

	//set up signal masks //TODO error check this
	signal_init();

	//if needed, open pid file handle before dropping privileges
	if(config.pid_file){
		pid_file = fopen(config.pid_file, "w");
		if(!pid_file){
			logprintf(LOG_ERROR, "Failed to open pidfile for writing\n");
		}
	}

	//drop privileges
	if(getuid() == 0 && args.drop_privileges){
		if(privileges_drop(config.privileges) < 0){
			config_free(&config);
			TLSSUPPORT(gnutls_global_deinit());
			exit(EXIT_FAILURE);
		}
	}
	else{
		logprintf(LOG_INFO, "Not dropping privileges%s\n", (args.drop_privileges ? " (Because you are not root)":""));
	}

	//detach from console
	if(args.daemonize && log_output(NULL) != stderr){
		logprintf(LOG_INFO, "Detaching from parent process\n");

		//flush the stream so we do not get everything twice
		fflush(log_output(NULL));

		//stop secondary log output
		log_verbosity(-1, false);

		switch(daemonize(pid_file)){
			case 0:
				break;
			case 1:
				logprintf(LOG_INFO, "Parent process going down\n");
				config_free(&config);
				TLSSUPPORT(gnutls_global_deinit());
				exit(EXIT_SUCCESS);
				break;
			case -1:
				config_free(&config);
				TLSSUPPORT(gnutls_global_deinit());
				exit(EXIT_FAILURE);
		}
	}
	else{
		logprintf(LOG_INFO, "Not detaching from console%s\n", (args.daemonize ? " (Because the log output stream is stderr)":""));
		if(pid_file){
			fclose(pid_file);
		}
	}

	//run core loop
	core_loop(config.listeners, &(config.database));

	//cleanup
	config_free(&config);
	TLSSUPPORT(gnutls_global_deinit());

	log_shutdown();
	return 0;
}
