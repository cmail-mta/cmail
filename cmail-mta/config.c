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

#ifndef CMAIL_NO_TLS
int config_trustfile(CONFIGURATION* config, char* directive, char* params){
	gnutls_certificate_set_x509_trust_file(config->settings.tls_credentials, params, GNUTLS_X509_FMT_PEM);
	return 0;
}
#endif

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

int config_announce(CONFIGURATION* config, char* directive, char* params){
	config->settings.helo_announce=common_strdup(params);
	return (config->settings.helo_announce)?0:-1;
}

int config_ports(CONFIGURATION* config, char* directive, char* params){
	char* tokenize_argument;
	char* token=NULL;
	int num_ports=0;
	REMOTE_PORT empty_port = {
		#ifndef CMAIL_NO_TLS
		.tls_mode = TLS_NONE,
		#endif
		.port = 0
	};

	REMOTE_PORT new_port;

	token=strtok_r(params, " ", &tokenize_argument);
	do{
		new_port=empty_port;
		if(token){
			new_port.port=strtoul(token, &token, 10);
			#ifndef CMAIL_NO_TLS
			if(!strcmp(token, "@tls")){
				new_port.tls_mode=TLS_ONLY;
			}
			else if(!strcmp(token, "@starttls")){
				new_port.tls_mode=TLS_NEGOTIATE;
			}
			#endif

			if(new_port.port){
				#ifndef CMAIL_NO_TLS
				logprintf(config->log, LOG_DEBUG, "Adding port %d: %d @ tls mode %s\n", num_ports, new_port.port, tls_modestring(new_port.tls_mode));
				#else
				logprintf(config->log, LOG_DEBUG, "Adding port %d in position %d\n", new_port.port, num_ports);
				#endif

				config->settings.port_list=realloc(config->settings.port_list, (num_ports+1)*sizeof(REMOTE_PORT));
				if(!config->settings.port_list){
					logprintf(config->log, LOG_ERROR, "Failed to allocate memory for port list\n");
					return -1;
				}
				config->settings.port_list[num_ports]=new_port;

				num_ports++;
			}
			else{
				logprintf(config->log, LOG_WARNING, "Invalid port config stanza\n");
			}
		}

		token=strtok_r(NULL, " ", &tokenize_argument);
	}
	while(token);

	//add sentinel port
	config->settings.port_list=realloc(config->settings.port_list, (num_ports+1)*sizeof(REMOTE_PORT));
	if(!config->settings.port_list){
		logprintf(config->log, LOG_ERROR, "Failed to allocate memory for port list\n");
		return -1;
	}
	config->settings.port_list[num_ports]=empty_port;

	return 0;
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
	if(!strncmp(line, "user", 4)||!strncmp(line, "group", 5)){
		return config_privileges(config, line, line+parameter);
	}

	else if(!strncmp(line, "database", 8)){
		return config_database(config, line, line+parameter);
	}

	else if(!strncmp(line, "verbosity", 9)||!strncmp(line, "logfile", 7)){
		return config_logger(config, line, line+parameter);
	}

	else if(!strncmp(line, "announce", 8)){
		return config_announce(config, line, line+parameter);
	}

	else if(!strncmp(line, "portprio", 8)){
		return config_ports(config, line, line+parameter);
	}

	else if(!strncmp(line, "interval", 8)){
		config->settings.check_interval=strtoul(line+parameter, NULL, 10);
		//TODO sanity check this
		return 0;
	}

	else if(!strncmp(line, "retries", 7)){
		config->settings.mail_retries=strtoul(line+parameter, NULL, 10);
		//TODO sanity check this
		return 0;
	}

	else if(!strncmp(line, "retryinterval", 13)){
		config->settings.retry_interval=strtoul(line+parameter, NULL, 10);
		//TODO sanity check this
		return 0;
	}

	else if(!strncmp(line, "tls_padding", 11)){
		config->settings.tls_padding=strtoul(line+parameter, NULL, 10);
		return 0;
	}

	#ifndef CMAIL_NO_TLS
	else if(!strncmp(line, "tls_trustfile", 13)){
		return config_trustfile(config, line, line+parameter);
	}
	#endif

	else if(!strncmp(line, "ratelimit", 9)){
		config->settings.rate_limit=strtoul(line+parameter, NULL, 10);
		return 0;
	}

	logprintf(config->log, LOG_ERROR, "Unknown configuration directive %s\n", line);
	return -1;
}

void config_free(CONFIGURATION* config){
	database_free(config->log, &(config->database));

	if(config->settings.helo_announce){
		free(config->settings.helo_announce);
	}

	if(config->settings.port_list){
		free(config->settings.port_list);
	}

	#ifndef CMAIL_NO_TLS
	gnutls_certificate_free_credentials(config->settings.tls_credentials);
	#endif

	if(config->log.stream!=stderr){
		fclose(config->log.stream);
		config->log.stream=stderr;
	}
}
