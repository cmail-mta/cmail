int imapstate_new(LOGGER log, IMAP_COMMAND sentence, CONNECTION* client, DATABASE* database){
	IMAP_COMMAND_STATE state = COMMAND_UNHANDLED;
	char* state_reason = NULL;
	int rv = 0;

	//commands valid in any state as per RFC 3501 6.1
	if(!strcasecmp(sentence.command, "capability")){
		client_send(log, client, "* CAPABILITY IMAP4rev1 STARTTLS LOGINDISABLED\r\n"); //TODO make this dynamic
		state = COMMAND_OK;
	}
	else if(!strcasecmp(sentence.command, "noop")){
		//this one is easy
		state = COMMAND_OK;
	}
	else if(!strcasecmp(sentence.command, "logout")){
		client_send(log, client, "* BYE for now\r\n");
		//this is kind of a hack as it bypasses the default command state responder
		client_send(log, client, "%s OK LOGOUT completed\r\n", sentence.tag);
		return client_close(log, client, database);
	}

	//actual NEW commands as per RFC 3501 6.2
	else if(!strcasecmp(sentence.command, "starttls")){
		//TODO implement
		state = COMMAND_NOREPLY;
	}
	else if(!strcasecmp(sentence.command, "authenticate")){
		//TODO implement
		state = COMMAND_NOREPLY;
	}
	else if(!strcasecmp(sentence.command, "login")){
		//TODO implement
		state = COMMAND_NOREPLY;
	}

	switch(state){
		case COMMAND_UNHANDLED:
			logprintf(log, LOG_WARNING, "Unhandled command %s in state NEW\n", sentence.command);
			client_send(log, client, "%s BAD Command not recognized\r\n", sentence.tag); //FIXME is this correct
			return -1;
		case COMMAND_OK:
			client_send(log, client, "%s OK %s\r\n", sentence.tag, state_reason ? state_reason:"Command completed");
			return rv;
		case COMMAND_BAD:
			client_send(log, client, "%s BAD %s\r\n", sentence.tag, state_reason ? state_reason:"Command failed");
			return rv;
		case COMMAND_NOREPLY:
			return rv;
	}

	logprintf(log, LOG_ERROR, "Illegal branch reached in state NEW\n");
	return -2;
}
