int args_parse(ARGUMENTS* args, int argc, char** argv){
	unsigned i;

	//FIXME set verbosity

	for(i=0;i<argc;i++){
		if(!strcmp(argv[i], "nodrop")){
			args->drop_privileges=false;
		}
		else if(!strcmp(argv[i], "nodetach")){
			args->daemonize=false;
		}
		else if(!strcmp(argv[i], "deliver")||!strcmp(argv[i], "handoff")){
			if(i+1<argc){
				if(argv[i][0]=='h'){
					args->remote.mode=DELIVER_HANDOFF;
				}
				printf("Updating delivery domain to %s\n", argv[i+1]);
				args->remote.host=argv[++i];
			}
			else{
				printf("Failed to read argument for delivery domain\n");
				return -1;
			}
		}
		else{
			args->config_file=argv[i];
		}
	}

	return (args->config_file)?0:-1;
}
