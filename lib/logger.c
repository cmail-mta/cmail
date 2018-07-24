/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

#include "logger.h"
#include "common.h"

struct /*_LOGGER*/ {
	FILE* stream;
	unsigned verbosity;
	bool log_secondary;
	bool print_timestamp;
	#ifdef LOGGER_MT_SAFE
	pthread_mutex_t* sync;
	#endif
} logger = {
	.stream = NULL,
	.verbosity = 0,
	.log_secondary = false,
	.print_timestamp = true
};

void logprintf(unsigned level, char* fmt, ...){
	va_list args;
	va_list copy;
	char timestring[LOGGER_TIMESTRING_LEN];

	va_start(args, fmt);

	if(!logger.stream){
		logger.stream = stderr;
	}

	if(logger.print_timestamp){
		if(common_tprintf("%a, %d %b %Y %T %z", time(NULL), timestring, sizeof(timestring) - 1) < 0){
			snprintf(timestring, sizeof(timestring) - 1, "Time failed");
		}
	}

	if(logger.log_secondary){
		if(logger.verbosity >= level){
			va_copy(copy, args);
			#ifdef LOGGER_MT_SAFE
			if(logger.sync){
				pthread_mutex_lock(logger.sync);
			}
			#endif
			if(logger.print_timestamp){
				fprintf(stderr, "%s ", timestring);
			}
			vfprintf(stderr, fmt, copy);
			fflush(stderr);
			#ifdef LOGGER_MT_SAFE
			if(logger.sync){
				pthread_mutex_unlock(logger.sync);
			}
			#endif
			va_end(copy);
		}
	}

	if(logger.verbosity >= level){
		#ifdef LOGGER_MT_SAFE
		if(logger.sync){
			pthread_mutex_lock(logger.sync);
		}
		#endif
		if(logger.print_timestamp){
			fprintf(logger.stream, "%s ", timestring);
		}
		vfprintf(logger.stream, fmt, args);
		fflush(logger.stream);
		#ifdef LOGGER_MT_SAFE
		if(logger.sync){
			pthread_mutex_unlock(logger.sync);
		}
		#endif
	}
	va_end(args);
}

void log_dump_buffer(unsigned level, void* buffer, size_t bytes){
	uint8_t* data = (uint8_t*)buffer;
	size_t i;

	logprintf(level, "Buffer dump (%d bytes)\n", bytes);

	for(i = 0; i < bytes; i++){
		logprintf(level, "Position %d: (%c, %02x)\n", i, isprint(data[i]) ? data[i]:'.', data[i]);
	}
}

void log_verbosity(int level, bool secondary){
	if(level >= 0){
		logger.verbosity = level;
	}

	logger.log_secondary = secondary;
}

FILE* log_output(FILE* stream){
	if(stream){
		logger.stream = stream;
	}

	if(!logger.stream){
		logger.stream = stderr;
	}

	return logger.stream;
}
