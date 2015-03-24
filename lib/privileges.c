int privileges_drop(LOGGER log, USER_PRIVS privileges){
	if(getuid()!=0){
		logprintf(log, LOG_WARNING, "Not dropping privileges, need root for that\n");
		return -1;
	}
	
	logprintf(log, LOG_INFO, "Dropping privileges...\n");
	
	//TODO initgroups
	
	if(chdir("/")<0){
		logprintf(log, LOG_ERROR, "Failed to drop privileges (changing directories): %s\n", strerror(errno));
		return -1;
	}
	
	if(setgid(privileges.gid) != 0){
		logprintf(log, LOG_ERROR, "Failed to drop privileges (changing gid): %s\n", strerror(errno));
		return -1;
	}
		
	if(setuid(privileges.uid) != 0){
		logprintf(log, LOG_ERROR, "Failed to drop privileges (changing uid): %s\n", strerror(errno));
		return -1;
	}
	
	//TODO check for success
	return 0;
}