int config_bind(CONFIGURATION* config, char* directive, char* params){
	char* tokenize_line = NULL;
	char* tokenize_argument = NULL;
	char* token = NULL;

	char* bindhost = NULL;
	char* port = NULL;

	#ifndef CMAIL_NO_TLS
	TLSMODE tls_mode=TLS_NONE;
	char* tls_keyfile = NULL;
	char* tls_certfile = NULL;
	char* tls_priorities = NULL;
	char* tls_dh_paramfile = NULL;
	#endif

	int listen_fd = -1;
	int listener_slot = -1;
	LISTENER settings = {
		.fixed_user = NULL,
		.max_size = 0,
		.announce_domain = "cmail-smtpd",
		.auth_offer = AUTH_NONE,
		.auth_require = false
	};
	LISTENER* listener_data = NULL;

	//tokenize line
	bindhost = strtok_r(params, " ", &tokenize_line);
	do{
		token=strtok_r(NULL, " ", &tokenize_line);
		if(token){
			if(!port){
				port = token;
			}
			#ifndef CMAIL_NO_TLS
			else if(!strncmp(token, "cert=", 5)){
				tls_certfile = token + 5;
			}
			else if(!strncmp(token, "key=", 4)){
				tls_keyfile = token + 4;
			}
			else if(!strcmp(token, "tlsonly")){
				tls_mode = TLS_ONLY;
			}
			else if(!strncmp(token, "ciphers=", 8)){
				tls_priorities = token + 8;
			}
			else if(!strncmp(token, "dhparams=", 9)){
				tls_dh_paramfile = token + 9;
			}
			#endif
			else if(!strncmp(token, "auth", 4)){
				settings.auth_offer = AUTH_ANY;
				if(token[4] == '='){
					token = strtok_r(token + 5, ",", &tokenize_argument);
					while(token){
						if(!strcmp(token, "tlsonly")){
							settings.auth_offer = AUTH_TLSONLY;
						}
						else if(!strcmp(token, "strict")){
							settings.auth_require = true;
						}
						else if(!strncmp(token, "fixed@", 6)){
							settings.auth_require = true;
							settings.fixed_user = token + 6;
						}
						else{
							logprintf(config->log, LOG_WARNING, "Unknown auth parameter %s\n", token);
						}
						token = strtok_r(NULL, ",", &tokenize_argument);
					}

					token = ""; //reset to anything but NULL to meet condition
				}
			}
			else if(!strncmp(token, "announce=", 9)){
				settings.announce_domain = token + 9;
			}
			else if(!strncmp(token, "size=", 5)){
				settings.max_size = strtoul(token + 5, NULL, 10);
			}
			else{
				logprintf(config->log, LOG_INFO, "Ignored additional bind parameter %s\n", token);
			}
		}
	}while(token);

	#ifndef CMAIL_NO_TLS
	if(tls_keyfile && tls_certfile){
		if(tls_mode == TLS_NONE){
			tls_mode = TLS_NEGOTIATE;
		}

		if(tls_init_listener(config->log, &settings, tls_certfile, tls_keyfile, tls_dh_paramfile, tls_priorities) < 0){
			return -1;
		}
	}
	else if(tls_keyfile || tls_certfile || tls_mode != TLS_NONE){
		logprintf(config->log, LOG_ERROR, "Need both certificate and key for TLS\n");
		return -1;
	}
	#endif

	#ifdef CMAIL_TEST_REPL
	if(!strcmp(bindhost, "repl")){
		logprintf(config->log, LOG_INFO, "Creating REPL listener\n");
		listen_fd = fileno(stderr);
	}
	else{
		//try to open a listening socket
		listen_fd = network_listener(config->log, bindhost, port);
	}
	#else
	//try to open a listening socket
	listen_fd = network_listener(config->log, bindhost, port);
	#endif

	if(listen_fd < 0){
		return -1;
	}

	//add the new listener to the pool
	listener_slot = connpool_add(&(config->listeners), listen_fd);
	if(listener_slot >= 0){
		logprintf(config->log, LOG_INFO, "Bound to %s port %s (slot %d)\n", bindhost, port, listener_slot);

		//create listener auxdata
		config->listeners.conns[listener_slot].aux_data = calloc(1, sizeof(LISTENER));
		if(!config->listeners.conns[listener_slot].aux_data){
			logprintf(config->log, LOG_ERROR, "Failed to allocate auxiliary data for listener\n");
			return -1;
		}

		listener_data = (LISTENER*)config->listeners.conns[listener_slot].aux_data;
		*listener_data = settings;

		//copy data to heap
		#ifndef CMAIL_NO_TLS
		config->listeners.conns[listener_slot].tls_mode = tls_mode;
		#endif

		listener_data->announce_domain = common_strdup(settings.announce_domain);
		if(!listener_data->announce_domain){
			logprintf(config->log, LOG_ERROR, "Failed to allocate auxiliary data for listener announce\n");
			return -1;
		}

		if(settings.fixed_user){
			listener_data->fixed_user = common_strdup(settings.fixed_user);
			if(!listener_data->fixed_user){
				logprintf(config->log, LOG_ERROR, "Failed to allocate memory for fixed user storage\n");
				return -1;
			}
		}
		return 0;
	}

	logprintf(config->log, LOG_ERROR, "Failed to store listen socket\n");
	return -1;
}

int config_privileges(CONFIGURATION* config, char* directive, char* params){
	struct passwd* user_info;
	struct group* group_info;

	if(!strcmp(directive, "user")){
		errno=0;
		user_info=getpwnam(params);
		if(!user_info){
			logprintf(config->log, LOG_ERROR, "Failed to get user info for %s\n", params);
			return -1;
		}
		config->privileges.uid=user_info->pw_uid;
		config->privileges.gid=user_info->pw_gid;
		logprintf(config->log, LOG_DEBUG, "Configured dropped privileges to uid %d gid %d\n", config->privileges.uid, config->privileges.gid);
		return 0;
	}
	else if(!strcmp(directive, "group")){
		errno=0;
		group_info=getgrnam(params);
		if(!group_info){
			logprintf(config->log, LOG_ERROR, "Failed to get group info for %s\n", params);
			return -1;
		}
		config->privileges.gid=group_info->gr_gid;
		logprintf(config->log, LOG_DEBUG, "Configured dropped privileges to gid %d\n", config->privileges.gid);
		return 0;
	}
	return -1;
}

int config_database(CONFIGURATION* config, char* directive, char* params){
	if(config->database.conn){
		logprintf(config->log, LOG_ERROR, "Can not use %s as master database, another one is already attached\n", params);
		return -1;
	}

	config->database.conn=database_open(config->log, params, SQLITE_OPEN_READWRITE);

	return (config->database.conn)?0:-1;
}

int config_logger(CONFIGURATION* config, char* directive, char* params){
	FILE* log_file;

	if(!strcmp(directive, "verbosity")){
		config->log.verbosity=strtoul(params, NULL, 10);
		return 0;
	}
	else if(!strcmp(directive, "logfile")){
		log_file=fopen(params, "a");
		if(!log_file){
			logprintf(config->log, LOG_ERROR, "Failed to open logfile %s for appending\n", params);
			return -1;
		}
		config->log.stream = log_file;
		config->log.log_secondary = true;
		return 0;
	}
	return -1;
}

int config_line(void* config_data, char* line){
	unsigned parameter;
	CONFIGURATION* config=(CONFIGURATION*) config_data;

	//scan over directive
	for(parameter=0;(!isspace(line[parameter]))&&line[parameter]!=0;parameter++){
	}

	if(line[parameter]!=0){
		line[parameter]=0;
		parameter++;
	}

	//scan for parameter begin
	for(;isspace(line[parameter]);parameter++){
	}

	//route directives
	if(!strncmp(line, "bind", 4)){
		return config_bind(config, line, line+parameter);
	}

	else if(!strncmp(line, "user", 4)||!strncmp(line, "group", 5)){
		return config_privileges(config, line, line+parameter);
	}

	else if(!strncmp(line, "database", 8)){
		return config_database(config, line, line+parameter);
	}

	else if(!strncmp(line, "verbosity", 9)||!strncmp(line, "logfile", 7)){
		return config_logger(config, line, line+parameter);
	}

	logprintf(config->log, LOG_ERROR, "Unknown configuration directive %s\n", line);
	return -1;
}

void config_free(CONFIGURATION* config){
	unsigned i;
	LISTENER* listener_data;

	for(i=0;i<config->listeners.count;i++){
		listener_data=(LISTENER*)config->listeners.conns[i].aux_data;

		close(config->listeners.conns[i].fd);

		free(listener_data->announce_domain);

		if(listener_data->fixed_user){
			free(listener_data->fixed_user);
		}

		#ifndef CMAIL_NO_TLS
		if(config->listeners.conns[i].tls_mode!=TLS_NONE){
			gnutls_certificate_free_credentials(listener_data->tls_cert);
			gnutls_priority_deinit(listener_data->tls_priorities);
			gnutls_dh_params_deinit(listener_data->tls_dhparams);
		}
		#endif
	}

	connpool_free(&(config->listeners));
	database_free(config->log, &(config->database));

	if(config->log.stream!=stderr){
		fflush(config->log.stream);
		fclose(config->log.stream);
		config->log.stream=stderr;
	}
}
