/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

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
			#ifdef LOGGER_MT_SAFE
			if(log.sync){
				pthread_mutex_lock(log.sync);
			}
			#endif
			if(log.print_timestamp){
				fprintf(stderr, "%s ", timestring);
			}
			vfprintf(stderr, fmt, copy);
			fflush(stderr);
			#ifdef LOGGER_MT_SAFE
			if(log.sync){
				pthread_mutex_unlock(log.sync);
			}
			#endif
			va_end(copy);
		}
	}

	if(log.verbosity >= level){
		#ifdef LOGGER_MT_SAFE
		if(log.sync){
			pthread_mutex_lock(log.sync);
		}
		#endif
		if(log.print_timestamp){
			fprintf(log.stream, "%s ", timestring);
		}
		vfprintf(log.stream, fmt, args);
		fflush(log.stream);
		#ifdef LOGGER_MT_SAFE
		if(log.sync){
			pthread_mutex_unlock(log.sync);
		}
		#endif
	}
	va_end(args);
}

void log_dump_buffer(LOGGER log, unsigned level, void* buffer, size_t bytes){
	uint8_t* data = (uint8_t*)buffer;
	size_t i;

	logprintf(log, level, "Buffer dump (%d bytes)\n", bytes);

	for(i = 0; i < bytes; i++){
		logprintf(log, level, "Position %d: (%c, %02x)\n", i, isprint(data[i]) ? data[i]:'.', data[i]);
	}
}
