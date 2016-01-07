int imap_create(LOGGER log, WORKER_DATABASE* db, char* mailbox){
	//decode mailbox name from UTF-7

	//create mailbox path iteratively

	return -1;
}

int imap_logout(LOGGER log, IMAP_COMMAND sentence, CONNECTION* client, DATABASE* database, COMMAND_QUEUE* queue){
	client_send(log, client, "* BYE for now\r\n");
	client_send(log, client, "%s OK LOGOUT completed\r\n", sentence.tag);
	//FIXME this needs the command queue mutex in order to ensure that the queue does not contain any more commands
	//for this connection
	return client_close(log, client, database, queue);
}

IMAP_COMMAND_STATE imap_capability(LOGGER log, IMAP_COMMAND sentence, CONNECTION* client, DATABASE* database){
	CLIENT* client_data = (CLIENT*)client->aux_data;
	LISTENER* listener_data = (LISTENER*)client_data->listener->aux_data;

	client_send(log, client, "* CAPABILITY IMAP4rev1");
	#ifndef CMAIL_NO_TLS
	if(client_data->listener->tls_mode == TLS_NEGOTIATE && client->tls_mode == TLS_NONE){
		client_send(log, client, " STARTTLS");
	}
	#endif
	if(!client_data->auth.user.authenticated && (
				(listener_data->auth_offer == AUTH_ANY) ||
				(listener_data->auth_offer == AUTH_TLSONLY && client->tls_mode == TLS_ONLY)
				)){
		client_send(log, client, " AUTH=PLAIN SASL-IR");
	}
	else{
		client_send(log, client, " LOGINDISABLED");
	}
	client_send(log, client, " LITERAL+ XYZZY\r\n");

	return COMMAND_OK;
}

IMAP_COMMAND_STATE imap_xyzzy(LOGGER log, IMAP_COMMAND sentence, CONNECTION* client, DATABASE* database){
	CLIENT* client_data = (CLIENT*)client->aux_data;
	logprintf(log, LOG_INFO, "Client performs incantation\n");
	#ifndef CMAIL_NO_TLS
	logprintf(log, LOG_DEBUG, "Client TLS status: %s\n", tls_modestring(client->tls_mode));
	#endif
	logprintf(log, LOG_DEBUG, "Auth state: %s, Method: %s\n", client_data->auth.user.authenticated ? "true":"false", client_data->auth.method == IMAP_LOGIN ? "LOGIN":"AUTHENTICATE");
	logprintf(log, LOG_DEBUG, "Authentication: %s, Authorization: %s\n", client_data->auth.user.authenticated ? client_data->auth.user.authenticated:"null", client_data->auth.user.authorized ? client_data->auth.user.authorized:"null");
	logprintf(log, LOG_DEBUG, "Current state: %s\n", client_data->state == STATE_NEW ? "NEW" : "AUTHENTICATED");
	logprintf(log, LOG_DEBUG, "Connection score: %d\n", client_data->connection_score);
	client_send(log, client, "* XYZZY Nothing happens\r\n");
	return COMMAND_OK;
}
