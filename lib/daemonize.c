#include <unistd.h>

int daemonize(LOGGER log, FILE* pidfile_handle){
	int pid = fork();
	if(pid < 0){
		logprintf(log, LOG_ERROR, "Failed to fork\n");
		if(pidfile_handle){
			fclose(pidfile_handle);
		}
		return -1;
	}

	if(pid == 0){
		close(0);
		close(1);
		close(2);
		setsid();
		if(pidfile_handle){
			fclose(pidfile_handle);
		}
		return 0;
	}

	else{
		if(pidfile_handle){
			fprintf(pidfile_handle, "%d\n", pid);
			fclose(pidfile_handle);
		}
		return 1;
	}
}
