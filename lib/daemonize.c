int daemonize(LOGGER log){
	int pid = fork();
	if(pid < 0){
		logprintf(log, LOG_ERROR, "Failed to fork\n");
		return -1;
	}

	if(pid == 0){
		close(0);
		close(1);
		close(2);
		setsid();
		return 0;
	}

	else{
		return 1;
	}
}