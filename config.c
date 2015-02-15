int config_bind(CONFIGURATION* config, char* params){
	//TODO
	fprintf(stderr, "PARAM %s", params);
	return -1;
}

int config_privileges(CONFIGURATION* config, char* params){
	//TODO
	fprintf(stderr, "PARAM %s", params);
	return -1;
}

int config_database(CONFIGURATION* config, char* params){
	//TODO
	fprintf(stderr, "PARAM %s", params);
	return -1;
}

int config_logger(CONFIGURATION* config, char* params){
	//TODO
	fprintf(stderr, "PARAM %s", params);
	return -1;
}

int config_line(CONFIGURATION* config, char* line){
	unsigned offset, parameter;

	for(offset=0;isspace(line[offset]);offset++){
	}

	//ignore empty lines & comments
	if(line[offset]!='#' && line[offset]!=0){
		//scan for parameter offset
		for(parameter=offset;(!isspace(line[parameter]))&&line[parameter]!=0;parameter++){
		}
		for(;isspace(line[parameter]);parameter++){
		}

		//route directives
		if(!strncmp(line+offset, "bind", 4)){
			return config_bind(config, line+parameter);
		}

		else if(!strncmp(line+offset, "user", 4)||!strncmp(line+offset, "group", 5)){
			return config_privileges(config, line+parameter);
		}

		else if(!strncmp(line+offset, "database", 8)){
			return config_database(config, line+parameter);
		}

		else if(!strncmp(line+offset, "verbosity", 9)||!strncmp(line+offset, "logfile", 7)){
			return config_logger(config, line+parameter);
		}

		else{
			//FIXME print only directive here
			logprintf(config->log, LOG_ERROR, "Unknown configuration directive %s\n", line);
			return -1;
		}
	}

	return 0;
}

int config_parse(CONFIGURATION* config, char* conf_file){
	char line_buffer[MAX_CFGLINE+1];
	FILE* conf_stream = fopen(conf_file, "r");

	if(!conf_stream){
		logprintf(config->log, LOG_ERROR, "Failed to read configuration file: %s\n", strerror(errno));
		return -1;
	}

	while(fgets(line_buffer, MAX_CFGLINE, conf_stream)!=NULL){
		if(config_line(config, line_buffer)<0){
			fclose(conf_stream);
			return -1;
		}
	}
	
	if(errno!=0){
		logprintf(config->log, LOG_ERROR, "Error while reading configuration file: %s\n", strerror(errno));
		fclose(conf_stream);
		return -1;
	}

	fclose(conf_stream);
	return 0;
}

void config_free(CONFIGURATION* config){
	//TODO
}
