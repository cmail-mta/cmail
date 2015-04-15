#ifndef CMAIL_NO_TLS

int tls_initclient(LOGGER log, CONNECTION* client, gnutls_priority_t tls_priorities, gnutls_certificate_credentials_t tls_cert){
	int status;

	status=gnutls_init(&(client->tls_session), GNUTLS_SERVER);
	if(status){
		logprintf(log, LOG_WARNING, "Failed to initialize TLS session for client: %s\n", gnutls_strerror(status));
		return -1;
	}

	status=gnutls_priority_set(client->tls_session, tls_priorities);
	if(status){
		logprintf(log, LOG_WARNING, "Failed to update priority set for client: %s\n", gnutls_strerror(status));
	}

	status=gnutls_credentials_set(client->tls_session, GNUTLS_CRD_CERTIFICATE, tls_cert);
	if(status){
		logprintf(log, LOG_WARNING, "Failed to set credentials for client: %s\n", gnutls_strerror(status));
		return -1;
	}

	gnutls_certificate_server_set_request(client->tls_session, GNUTLS_CERT_IGNORE);
	
	gnutls_transport_set_int(client->tls_session, client->fd);

	return 0;
}

int tls_initserver(LOGGER log, LISTENER* listener, char* cert, char* key, char* priorities){
		logprintf(log, LOG_DEBUG, "Initializing TLS priorities\n");
		if(gnutls_priority_init(&(listener->tls_priorities), (priorities)?priorities:"PERFORMANCE:%SERVER_PRECEDENCE", NULL)){
			logprintf(log, LOG_ERROR, "Failed to initialize TLS priorities\n");
			return -1;
		}

		logprintf(log, LOG_DEBUG, "Initializing TLS certificate structure\n");
		if(gnutls_certificate_allocate_credentials(&(listener->tls_cert))){
			logprintf(log, LOG_ERROR, "Failed to allocate storage for TLS cert structure\n");
			return -1;
		}
		
		logprintf(log, LOG_DEBUG, "Initializing TLS certificate\n");
		if(gnutls_certificate_set_x509_key_file(listener->tls_cert, cert, key, GNUTLS_X509_FMT_PEM)){
			logprintf(log, LOG_ERROR, "Failed to find key or certificate files\n");
			return -1;
		}

		//FIXME error check this lot
		logprintf(log, LOG_DEBUG, "Generating Diffie-Hellman parameters\n");
        	gnutls_dh_params_init(&(listener->tls_dhparams));
	        gnutls_dh_params_generate2(listener->tls_dhparams, gnutls_sec_param_to_pk_bits(GNUTLS_PK_DH, GNUTLS_SEC_PARAM_LOW));
		gnutls_certificate_set_dh_params(listener->tls_cert, listener->tls_dhparams);
		return 0;
}


#endif
