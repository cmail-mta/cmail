void logprintf(LOGGER log, unsigned level, char* fmt, ...){
	va_list args;
	va_list copy;
	char timestring[LOGGER_TIMESTRING_LEN];

	va_start(args, fmt);

	if(log.print_timestamp){
		if(common_tprintf("%a, %d %b %Y %T %z", time(NULL), timestring, sizeof(timestring) - 1) < 0){
			snprintf(timestring, sizeof(timestring)-1, "Time failed");
		}
	}

	if(log.log_secondary){
		if(log.verbosity >= level){
			va_copy(copy, args);
			if(log.print_timestamp){
				fprintf(stderr, "%s ", timestring);
			}
			vfprintf(stderr, fmt, copy);
			fflush(stderr);
			va_end(copy);
		}
	}

	if(log.verbosity >= level){
		if(log.print_timestamp){
			fprintf(log.stream, "%s ", timestring);
		}
		vfprintf(log.stream, fmt, args);
		fflush(log.stream);
	}
	va_end(args);
}

void log_dump_buffer(LOGGER log, unsigned level, void* buffer, size_t bytes){
	uint8_t* data = (uint8_t*)buffer;
	size_t i;

	logprintf(log, level, "Buffer dump (%d bytes)\n", bytes);

	for(i=0;i<bytes;i++){
		logprintf(log, level, "(%d, %c, %02x)", i, isprint(data[i]) ? data[i]:'.', data[i]);
		if(i && i%8 == 0){
			logprintf(log, level, "\n");
		}
	}

	logprintf(log, level, "\n");
}
