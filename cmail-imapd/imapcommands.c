int imap_logout(LOGGER log, IMAP_COMMAND sentence, CONNECTION* client, DATABASE* database){
	client_send(log, client, "* BYE for now\r\n");
	client_send(log, client, "%s OK LOGOUT completed\r\n", sentence.tag);
	//FIXME this needs the command queue mutex in order to ensure that the queue does not contain any more commands
	//for this connection
	return client_close(log, client, database);
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
	client_send(log, client, "* XYZZY Nothing happens\r\n");
	return COMMAND_OK;
}
