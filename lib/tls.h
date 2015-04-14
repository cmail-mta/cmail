#ifndef CMAIL_NO_TLS
	#include <gnutls/gnutls.h>
	#define TLSSUPPORT(x) (x)
	typedef enum /*_TLS_MODE*/ {
		TLS_ONLY,		//Listener: TLS-Only port, Client: TLS Session active
		TLS_NEGOTIATE,		//Listener: STARTTLS enabled, Client: Handshake in progress
		TLS_NONE		//Listener: No TLS, Client: No TLS session active
	} TLSMODE;
	
	char* tls_modestring(TLSMODE mode){
		switch(mode){
			case TLS_NONE:
				return "TLS_NONE";
			case TLS_NEGOTIATE:
				return "TLS_NEGOTIATE";
			case TLS_ONLY:
				return "TLS_ONLY";
		}
		return "Unknown";
	}
#else
	#define TLSSUPPORT(x)
#endif