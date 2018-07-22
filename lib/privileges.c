/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

int privileges_drop(USER_PRIVS privileges){
	if(getuid()!=0){
		logprintf(LOG_WARNING, "Not dropping privileges, need root for that\n");
		return -1;
	}

	logprintf(LOG_INFO, "Dropping privileges...\n");

	//TODO initgroups

	if(chdir("/")<0){
		logprintf(LOG_ERROR, "Failed to drop privileges (changing directories): %s\n", strerror(errno));
		return -1;
	}

	if(setgid(privileges.gid) != 0){
		logprintf(LOG_ERROR, "Failed to drop privileges (changing gid): %s\n", strerror(errno));
		return -1;
	}

	if(setuid(privileges.uid) != 0){
		logprintf(LOG_ERROR, "Failed to drop privileges (changing uid): %s\n", strerror(errno));
		return -1;
	}

	//TODO check for success
	return 0;
}
