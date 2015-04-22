void logprintf(LOGGER log, unsigned level, char* fmt, ...){
	va_list args;

	va_start(args, fmt);
	if(log.verbosity>=level){
		vfprintf(log.stream, fmt, args);
		fflush(log.stream);
	}
	va_end(args);
}

void log_dump_buffer(LOGGER log, unsigned level, void* buffer, size_t bytes){
	uint8_t* data=(uint8_t*)buffer;
	size_t i;

	logprintf(log, level, "Buffer dump (%d bytes)\n", bytes);

	for(i=0;i<bytes;i++){
		logprintf(log, level, "(%d, %c, %02x)", i, isprint(data[i])?data[i]:'.', data[i]);
		if(i && i%8==0){
			logprintf(log, level, "\n");
		}
	}

	logprintf(log, level, "\n");
}

