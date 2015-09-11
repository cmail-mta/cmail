int control_process(LOGGER log, CONTROLPIPE pipe){
	char pipe_buffer[PIPE_BUF];
	ssize_t bytes = 0;

	bytes = read(pipe.input, pipe_buffer, sizeof(pipe_buffer) - 1);
	if(bytes <= 0){
		logprintf(log, LOG_ERROR, "Failed to read from control pipe: %s\n", strerror(errno));
		return -1;
	}

	logprintf(log, LOG_DEBUG, "Received %d bytes on control pipe\n", bytes);
	pipe_buffer[bytes] = 0;

	if(!strncmp(pipe_buffer, "echo ", 5)){
		if(pipe.output >= 0){
			write(pipe.output, pipe_buffer + 5,  bytes - 5);
		}
	}

	return 0;
}
