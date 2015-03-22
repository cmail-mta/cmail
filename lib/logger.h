typedef struct /*_LOGGER*/ {
	FILE* stream;
	unsigned verbosity;
} LOGGER;

#define LOG_ERROR 	0
#define LOG_WARNING 	0
#define LOG_INFO 	1
#define LOG_DEBUG 	3
#define LOG_ALL_IO	4