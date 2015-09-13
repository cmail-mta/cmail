#include <unistd.h>

int daemonize(LOGGER log, char* pid_file){
	FILE* pidfile_handle;
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
		if(pid_file){
			pidfile_handle = fopen(pid_file, "w");
			if(!pidfile_handle){
				logprintf(log, LOG_ERROR, "Failed to open pidfile for writing\n");
				return -1;
			}
			fprintf(pidfile_handle, "%d\n", pid);
			fclose(pidfile_handle);
		}
		return 1;
	}
}
