#ifndef CMAIL_NO_TLS
	#include <gnutls/gnutls.h>
	#define TLSSUPPORT(x) (x)
	typedef enum /*_TLS_MODE*/ {
		TLS_ONLY,		//Listener: TLS-Only port, Client: TLS Session active
		TLS_NEGOTIATE,		//Listener: STARTTLS enabled, Client: Handshake in progress
		TLS_NONE		//Listener: No TLS, Client: No TLS session active
	} TLSMODE;
#else
	#define TLSSUPPORT(x)
#endif