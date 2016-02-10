int imap_create(LOGGER log, WORKER_DATABASE* db, QUEUED_COMMAND* command, char* user, char* mailbox){
	int rv = 0;
	size_t mailbox_len = strlen(mailbox), i;
	char* next_mailbox = NULL;
	char* tokenize_mailbox;
	int mailbox_id = -1, parent;

	//TODO this should modfy the connection score

	//decode mailbox name from UTF-7
	//path_len = protocol_utf7_decode(log, mailbox);
	//if(path_len < 0){
	//	logprintf(log, LOG_ERROR, "Failed to decode mailbox path\n");
	//	//TODO send appropriate response
	//	return -1;
	//}

	logprintf(log, LOG_DEBUG, "Trying to create mailbox path %s\n", mailbox);

	parent = database_resolve_path(log, db, user, mailbox, &next_mailbox);
	if(parent < 0){
		logprintf(log, LOG_ERROR, "Invalid path provided\n");
		command->replies = common_strappf(command->replies, &(command->replies_length),
				"%s NO Invalid path specified\r\n", command->tag);
		return -1;
	}

	//check if entire path existed before creating it
	if(!next_mailbox){
		logprintf(log, LOG_WARNING, "Path already existed\n");
		command->replies = common_strappf(command->replies, &(command->replies_length),
				"%s NO Mailbox already exists\r\n", command->tag);
		return -1;
	}

	//create mailbox path iteratively
	for(next_mailbox = strtok_r(next_mailbox, "/", &tokenize_mailbox); next_mailbox; next_mailbox = strtok_r(NULL, "/", &tokenize_mailbox)){
		logprintf(log, LOG_DEBUG, "Creating mailbox %s parent %d\n", next_mailbox, parent);

		mailbox_id = database_create_mailbox(log, db, user, next_mailbox, parent);
		if(mailbox_id < 0){
			//FIXME might want to rollback changes
			logprintf(log, LOG_ERROR, "Failed to create mailbox\n");
			command->replies = common_strappf(command->replies, &(command->replies_length),
					"%s NO Failed to create mailbox\r\n", command->tag);
			rv = -1;
			break;
		}

		parent = mailbox_id;
	}

	//reset all terminators in the mailbox path
	for(i = 0; i < mailbox_len; i++){
		if(mailbox[i] == 0){
			mailbox[i] = '/';
		}
	}

	return rv;
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
