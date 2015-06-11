typedef enum /*_SASL_METHOD*/ {
	SASL_INVALID,
	SASL_PLAIN
} SASL_METHOD;

typedef struct /*_SASL_USER*/ {
	char* authenticated;
	char* authorized;
} SASL_USER;

typedef struct /*_SASL_CONTEXT*/ {
	SASL_METHOD method;
	void* method_data;
	SASL_USER* user;
} SASL_CONTEXT;

enum /*_SASL_STATUS*/ {
	SASL_OK = 0,
	SASL_ERROR_PROCESSING,
	SASL_ERROR_DATA,
	SASL_UNKNOWN_METHOD,
	SASL_CONTINUE,
	SASL_DATA_OK
};
