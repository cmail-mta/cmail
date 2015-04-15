#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "tls.h"

#define LISTEN_QUEUE_LENGTH 		128
#define MAX_SEND_CHUNK			2048

typedef struct /*_CONNECTION*/ {
	int fd;
	void* aux_data;
	#ifndef CMAIL_NO_TLS
	TLSMODE tls_mode;
	gnutls_session_t tls_session;
	#endif
} CONNECTION;
