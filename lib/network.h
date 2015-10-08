/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

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
