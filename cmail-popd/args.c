bool args_parse(ARGUMENTS* args, int argc, char** argv){
	unsigned i;

	//FIXME set verbosity

	for(i=0;i<argc;i++){
		if(!strcmp(argv[i], "nodrop")){
			args->drop_privileges=false;
		}
		else if(!strcmp(argv[i], "nodetach")){
			args->daemonize=false;
		}
		else{
			args->config_file=argv[i];
		}
	}

	return args->config_file!=NULL;
}
