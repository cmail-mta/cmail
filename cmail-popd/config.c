int config_bind(CONFIGURATION* config, char* directive, char* params){
	char* tokenize_line=NULL;
	char* token=NULL;

	char* bindhost=NULL;
	char* port=NULL;

	int listener_slot=-1;
	LISTENER settings = {
		.announce_domain = "cmail-popd",
	};
	LISTENER* listener_data=NULL;

	//tokenize line
	bindhost=strtok_r(params, " ", &tokenize_line);
	do{
		token=strtok_r(NULL, " ", &tokenize_line);
		if(token){
			if(!port){
				port=token;
			}
			else if(!strncmp(token, "announce=", 9)){
				settings.announce_domain=token+9;
			}
			else{
				logprintf(config->log, LOG_INFO, "Ignored additional bind parameter %s\n", token);
			}
		}
	}while(token);

	//try to open a listening socket
	int listen_fd=network_listener(config->log, bindhost, port);

	if(listen_fd<0){
		return -1;
	}

	//add the new listener to the pool
	listener_slot=connpool_add(&(config->listeners), listen_fd);
	if(listener_slot>=0){
		logprintf(config->log, LOG_INFO, "Bound to %s port %s (slot %d)\n", bindhost, port, listener_slot);

		//create listener auxdata
		config->listeners.conns[listener_slot].aux_data=calloc(1, sizeof(LISTENER));
		if(!config->listeners.conns[listener_slot].aux_data){
			logprintf(config->log, LOG_ERROR, "Failed to allocate auxiliary data for listener\n");
			return -1;
		}

		listener_data=(LISTENER*)config->listeners.conns[listener_slot].aux_data;
		*listener_data=settings;

		//copy data to heap
		listener_data->announce_domain=common_strdup(settings.announce_domain);
		if(!listener_data->announce_domain){
			logprintf(config->log, LOG_ERROR, "Failed to allocate auxiliary data for listener announce\n");
			return -1;
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

	switch(sqlite3_open_v2(params, &(config->database.conn), SQLITE_OPEN_READWRITE, NULL)){
		case SQLITE_OK:
			logprintf(config->log, LOG_DEBUG, "Attached %s as master database\n", params);
			return 0;
		default:
			logprintf(config->log, LOG_ERROR, "Failed to open %s as master databases\n", params);

	}
	
	return -1;
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
		config->log.stream=log_file;
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
		free(listener_data->announce_domain);
	}

	connpool_free(&(config->listeners));
	database_free(config->log, &(config->database));

	if(config->log.stream!=stderr){
		fclose(config->log.stream);
		config->log.stream=stderr;
	}
}
