int config_parse(LOGGER log, void* config_data, char* conf_file){
	char line_buffer[MAX_CFGLINE+1];
	char* line_data=NULL;
	int offset;

	if(!conf_file){
		logprintf(log, LOG_ERROR, "Null pointer passed as config file\n");
		return -1;
	}

	FILE* conf_stream = fopen(conf_file, "r");

	if(!conf_stream){
		logprintf(log, LOG_ERROR, "Failed to read configuration file: %s\n", strerror(errno));
		return -1;
	}

	while(fgets(line_buffer, MAX_CFGLINE, conf_stream)!=NULL){
		//preprocess line
		line_data=line_buffer;

		//remove trailing characters
		for(offset=strlen(line_data)-1;offset>=0&&!(isalnum(line_data[offset])||ispunct(line_data[offset]));offset--){
			line_data[offset]=0;
		}

		//skip leading spaces
		for(;isspace(line_data[0]);line_data++){
		}

		//handle inline commecnts
		for(offset=0;line_data[offset];offset++){
			if(line_data[offset]=='#'){
				line_data[offset]=0;
				break;
			}
		}

		//ignore empty lines & comments
		if(line_data[0]){
			//handle configuration
			if(config_line(config_data, line_data)<0){
				fclose(conf_stream);
				return -1;
			}
		}
	}

	if(errno!=0){
		logprintf(log, LOG_ERROR, "Error while reading configuration file: %s\n", strerror(errno));
		fclose(conf_stream);
		return -1;
	}

	fclose(conf_stream);
	return 0;
}
