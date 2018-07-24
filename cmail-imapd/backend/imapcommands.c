int imap_create(WORKER_DATABASE* db, QUEUED_COMMAND* command, char* user, char* mailbox){
	int rv = 0;
	char* next_mailbox = NULL;
	int parent = -1;

	//TODO this should modify the connection score
	logprintf(LOG_DEBUG, "Trying to create mailbox %s for %s\n", mailbox, user);

	//check if entire path existed before creating it (6.3.3: CREATE INBOX -> NO, CREATE EXISTING -> NO)
	if(database_query_mailbox(db, user, mailbox) >= 0){
		logprintf(LOG_WARNING, "Path already existed\n");
		command->replies = common_strappf(command->replies, &(command->replies_length),
				"%s NO Mailbox already exists\r\n", command->tag);
		return -1;
	}

	//create mailbox path iteratively
	for(next_mailbox = strchr(mailbox, '/'); next_mailbox; next_mailbox = strchr(next_mailbox + 1, '/')){
		*next_mailbox = 0;
		parent = database_query_mailbox(db, user, mailbox);
		if(parent >= 0){
			logprintf(LOG_DEBUG, "Reusing existing mailbox %s with ID\n", next_mailbox, parent);
		}
		else{
			parent = database_create_mailbox(db, user, mailbox, parent);
			if(parent < 0){
				//FIXME might want to rollback changes
				logprintf(LOG_ERROR, "Failed to create mailbox %s for user %s\n", mailbox, user);
				command->replies = common_strappf(command->replies, &(command->replies_length),
						"%s NO Failed to create mailbox\r\n", command->tag);
				rv = -1;
				break;
			}
			logprintf(LOG_DEBUG, "Created mailbox %s for %s as %d\n", next_mailbox, user, parent);
		}

		*next_mailbox = '/';
	}

	parent = database_create_mailbox(db, user, mailbox, parent);
	if(parent < 0){
		//FIXME might want to rollback changes
		logprintf(LOG_ERROR, "Failed to create mailbox %s for user %s\n", mailbox, user);
		command->replies = common_strappf(command->replies, &(command->replies_length),
			"%s NO Failed to create mailbox\r\n", command->tag);
		rv = -1;
	}
	logprintf(LOG_DEBUG, "Created mailbox path %s for %s as %d\n", mailbox, user, parent);
	return rv;
}

int imap_delete(WORKER_DATABASE* db, QUEUED_COMMAND* command, char* user, char* mailbox){
	int mailbox_id = database_query_mailbox(db, user, mailbox);
	//6.3.4: DELETE INBOX -> NO, DELETE NONEXISTING -> NO
	//Delete nonexisting might happen when either user or master database do not know that name
	if(!strcasecmp(mailbox, "INBOX") || mailbox_id < 0){
		logprintf(LOG_ERROR, "Failed to delete mailbox %s for user %s, does not exist\n", mailbox, user);
		command->replies = common_strappf(command->replies, &(command->replies_length),
				"%s NO Failed to delete mailbox\r\n", command->tag);
		return -1;
	}

	//6.3.4: Since we don't like \Noselect (and we want to keep the foreign key design), we don't delete mailboxes with inferiors
	if(database_mailbox_inferiors(db, mailbox_id, user, NULL) > 0){
		logprintf(LOG_ERROR, "Failed to delete mailbox %s for user %s, has inferiors\n", mailbox, user);
		command->replies = common_strappf(command->replies, &(command->replies_length),
				"%s NO Mailbox has inferiors\r\n", command->tag);
		return -1;
	}

	//RFC2683 implies that DELETE should also purge the mailbox
	if(database_delete_mailbox_mail(db, mailbox_id, user)){
		logprintf(LOG_ERROR, "Failed to purge mailbox %s for user %s\n", mailbox, user);
		command->replies = common_strappf(command->replies, &(command->replies_length),
				"%s NO Failed to delete mailbox contents\r\n", command->tag);
		return -2;
	}

	if(database_delete_mailbox(db, mailbox_id, user)){
		logprintf(LOG_ERROR, "Failed to delete mailbox entry %s for user %s\n", mailbox, user);
		command->replies = common_strappf(command->replies, &(command->replies_length),
				"%s NO Failed to delete mailbox\r\n", command->tag);
		return -2;
	}

	logprintf(LOG_ERROR, "Deleted mailbox %s for user %s\n", mailbox, user);
	return 0;
}

int imap_logout(IMAP_COMMAND sentence, CONNECTION* client, DATABASE* database, COMMAND_QUEUE* queue){
	client_send(client, "* BYE for now\r\n");
	client_send(client, "%s OK LOGOUT completed\r\n", sentence.tag);
	//client_close checks the command queue for lingering commands and marks them for discard
	return client_close(client, database, queue);
}

IMAP_COMMAND_STATE imap_capability(IMAP_COMMAND sentence, CONNECTION* client, DATABASE* database){
	CLIENT* client_data = (CLIENT*)client->aux_data;
	LISTENER* listener_data = (LISTENER*)client_data->listener->aux_data;

	client_send(client, "* CAPABILITY IMAP4rev1");
	#ifndef CMAIL_NO_TLS
	if(client_data->listener->tls_mode == TLS_NEGOTIATE && client->tls_mode == TLS_NONE){
		client_send(client, " STARTTLS");
	}
	#endif
	if(!client_data->auth.user.authenticated && (
				(listener_data->auth_offer == AUTH_ANY) ||
				(listener_data->auth_offer == AUTH_TLSONLY && client->tls_mode == TLS_ONLY)
				)){
		client_send(client, " AUTH=PLAIN SASL-IR");
	}
	else{
		client_send(client, " LOGINDISABLED");
	}
	client_send(client, " LITERAL+ XYZZY\r\n");

	return COMMAND_OK;
}

IMAP_COMMAND_STATE imap_xyzzy(IMAP_COMMAND sentence, CONNECTION* client, DATABASE* database){
	CLIENT* client_data = (CLIENT*)client->aux_data;
	logprintf(LOG_INFO, "Client performs incantation\n");
	#ifndef CMAIL_NO_TLS
	logprintf(LOG_DEBUG, "Client TLS status: %s\n", tls_modestring(client->tls_mode));
	#endif
	logprintf(LOG_DEBUG, "Auth state: %s, Method: %s\n", client_data->auth.user.authenticated ? "true":"false", client_data->auth.method == IMAP_LOGIN ? "LOGIN":"AUTHENTICATE");
	logprintf(LOG_DEBUG, "Authentication: %s, Authorization: %s\n", client_data->auth.user.authenticated ? client_data->auth.user.authenticated:"null", client_data->auth.user.authorized ? client_data->auth.user.authorized:"null");
	logprintf(LOG_DEBUG, "Current state: %s\n", client_data->state == STATE_NEW ? "NEW" : "AUTHENTICATED");
	logprintf(LOG_DEBUG, "Connection score: %d\n", client_data->connection_score);
	client_send(client, "* XYZZY Nothing happens\r\n");
	return COMMAND_OK;
}
