bool arguments_parse(ARGUMENTS* args, int argc, char** argv){
	//TODO
	return false;
}

void arguments_free(ARGUMENTS* args){
	if(args->config_file){
		free(args->config_file);
	}
}
