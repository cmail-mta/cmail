int imapstate_sasl(LOGGER log, COMMAND_QUEUE* command_queue, IMAP_COMMAND sentence, CONNECTION* client, DATABASE* database){
	CLIENT* client_data = (CLIENT*)client->aux_data;
	int sasl_status = SASL_OK;
	char* sasl_challenge = NULL;
	char* sasl_ir = NULL;
	IMAP_COMMAND temporary_command;
	unsigned i;

	if(!strcmp(client_data->recv_buffer, "*")){
		logprintf(log, LOG_INFO, "Client requested SASL cancellation\n");
		sasl_cancel(&(client_data->auth.ctx));
		client_send(log, client, "%s BAD Authentication request cancelled\r\n", client_data->auth.auth_tag ? client_data->auth.auth_tag:"*");
		auth_reset(&(client_data->auth));
		client_data->state = STATE_NEW;
	}
	else if(client_data->auth.method != IMAP_AUTHENTICATE && sentence.command && !strcasecmp(sentence.command, "authenticate")){
		if(!sentence.parameters){
			logprintf(log, LOG_WARNING, "Client tried AUTHENTICATE without supplying method\n");
			sasl_status = SASL_UNKNOWN_METHOD;
		}
		else{
			//scan for SASL initial response
			for(i = 0; sentence.parameters[i] && !isspace(sentence.parameters[i]); i++){
			}

			if(sentence.parameters[i] == ' '){
				sentence.parameters[i] = 0;
				sasl_ir = sentence.parameters + i + 1;
			}

			logprintf(log, LOG_DEBUG, "Beginning SASL with method %s\n", sentence.parameters, NULL);
			sasl_status = sasl_begin(log, &(client_data->auth.ctx), &(client_data->auth.user), sentence.parameters, sasl_ir, &sasl_challenge);
			client_data->auth.auth_tag = common_strdup(sentence.tag);
			if(!client_data->auth.auth_tag){
				logprintf(log, LOG_ERROR, "Failed to allocate memory to store authentication request tag\n");
				auth_reset(&(client_data->auth));
				client_send(log, client, "%s BAD Internal Error\r\n", sentence.tag);
				return 0;
			}
			else{
				client_data->auth.method = IMAP_AUTHENTICATE;
				client_data->state = STATE_SASL;
			}
		}
	}
	else{
		sasl_status = sasl_continue(log, &(client_data->auth.ctx), client_data->recv_buffer, &sasl_challenge);
	}

	switch(sasl_status){
		case SASL_OK:
			logprintf(log, LOG_ERROR, "SASL reply could not be handled by any branch\n");
			client_data->state = STATE_NEW;
			client_send(log, client, "%s BAD Invalid SASL state\r\n", client_data->auth.auth_tag ? client_data->auth.auth_tag:sentence.tag);
			auth_reset(&(client_data->auth));
			return -1;
		case SASL_ERROR_PROCESSING:
			logprintf(log, LOG_ERROR, "SASL processing error\r\n");
			client_send(log, client, "%s BAD Failed processing the SASL request\r\n", client_data->auth.auth_tag ? client_data->auth.auth_tag:sentence.tag);
			client_data->state = STATE_NEW;
			auth_reset(&(client_data->auth));
			return -1;
		case SASL_ERROR_DATA:
			logprintf(log, LOG_ERROR, "SASL failed to parse data\r\n");
			client_send(log, client, "%s BAD Invalid data provided\r\n", client_data->auth.auth_tag ? client_data->auth.auth_tag:sentence.tag);
			client_data->state = STATE_NEW;
			auth_reset(&(client_data->auth));
			return -1;
		case SASL_UNKNOWN_METHOD:
			logprintf(log, LOG_ERROR, "Client tried unknown SASL method\r\n");
			client_send(log, client, "%s NO Unknown SASL method\r\n", client_data->auth.auth_tag ? client_data->auth.auth_tag:sentence.tag);
			auth_reset(&(client_data->auth));
			client_data->state = STATE_NEW;
			return -1;
		case SASL_CONTINUE:
			logprintf(log, LOG_INFO, "Asking for SASL continuation\r\n");
			client_send(log, client, "+ %s\r\n", sasl_challenge ? sasl_challenge:"");
			return 0;
		case SASL_DATA_OK:
			sasl_reset_ctx(&(client_data->auth.ctx), true);

			//check provided authentication data
			if(!sasl_challenge || auth_validate(log, database, client_data->auth.user.authenticated, sasl_challenge, &(client_data->auth.user.authorized)) < 0){
				//login failed
				logprintf(log, LOG_INFO, "Client failed to authenticate\n");
				client_send(log, client, "%s NO Authentication failed\r\n", client_data->auth.auth_tag);
				auth_reset(&(client_data->auth));
				return -1;
			}

			if(!client_data->auth.user.authorized){
				logprintf(log, LOG_ERROR, "Failed to allocate memory for authorized user\n");
				client_send(log, client, "%s BAD Internal allocation error\r\n", client_data->auth.auth_tag);
				auth_reset(&(client_data->auth));
				return 0;
			}

			logprintf(log, LOG_INFO, "Client authenticated as user %s\n", client_data->auth.user.authenticated);

			//enqueue command to acquire connection specific data
			//as the command sentence may be split in this case, re-build a sentence that is valid in any case
			temporary_command.backing_buffer_length = (strlen(client_data->auth.auth_tag) + strlen("AUTHENTICATE") + 2) * sizeof(char);
			temporary_command.backing_buffer = calloc(temporary_command.backing_buffer_length, sizeof(char));
			if(!temporary_command.backing_buffer){
				logprintf(log, LOG_ERROR, "Failed to allocate memory for temporary command\n");
				client_send(log, client, "%s BAD Internal allocation error\r\n", client_data->auth.auth_tag);
				auth_reset(&(client_data->auth));
				return 0;
			}
			snprintf(temporary_command.backing_buffer, temporary_command.backing_buffer_length, "%s", client_data->auth.auth_tag);
			snprintf(temporary_command.backing_buffer + strlen(client_data->auth.auth_tag) + 1, temporary_command.backing_buffer_length - strlen(client_data->auth.auth_tag) - 1, "AUTHENTICATE");
			temporary_command.tag = temporary_command.backing_buffer;
			temporary_command.command = temporary_command.backing_buffer + strlen(client_data->auth.auth_tag) + 1;
			temporary_command.parameters = NULL;
			//enqueue it
			commandqueue_enqueue_command(log, command_queue, client, temporary_command, NULL);
			//clean it up again
			free(temporary_command.backing_buffer);
			return 1;
	}

	logprintf(log, LOG_ERROR, "Invalid branch reached in state SASL\n");
	return -1;
}

int imapstate_new(LOGGER log, COMMAND_QUEUE* command_queue, IMAP_COMMAND sentence, CONNECTION* client, DATABASE* database){
	IMAP_COMMAND_STATE state = COMMAND_UNHANDLED;
	CLIENT* client_data = (CLIENT*)client->aux_data;
	LISTENER* listener_data = (LISTENER*)client_data->listener->aux_data;
	char* state_reason = NULL;
	int rv = 0;
	ssize_t i;

	//commands valid in any state as per RFC 3501 6.1
	if(!strcasecmp(sentence.command, "capability")){
		state = imap_capability(log, sentence, client, database);
	}
	else if(!strcasecmp(sentence.command, "noop")){
		//this one is easy
		state = COMMAND_OK;
	}
	else if(!strcasecmp(sentence.command, "logout")){
		//this is kind of a hack as it bypasses the default command state responder
		return imap_logout(log, sentence, client, database, command_queue);
	}
	else if(!strcasecmp(sentence.command, "xyzzy")){
		state_reason = "Incantation performed";
		state = imap_xyzzy(log, sentence, client, database);
	}

	//actual NEW commands as per RFC 3501 6.2
	#ifndef CMAIL_NO_TLS
	else if(!strcasecmp(sentence.command, "starttls")){
		//accept only when offered, reject when already negotiated
		if(client->tls_mode != TLS_NONE){
			logprintf(log, LOG_WARNING, "Client with active TLS session tried to negotiate\n");

			state_reason = "TLS session already negotiated";
			state = COMMAND_BAD;
			//increase failscore
			rv = -1;
		}

		if(client_data->listener->tls_mode != TLS_NEGOTIATE){
			logprintf(log, LOG_WARNING, "Client tried to negotiate TLS with non-negotiable listener\n");

			state_reason = "TLS not possible now";
			state = COMMAND_BAD;
			//increase the failscore
			rv = -1;
		}

		if(state == COMMAND_UNHANDLED){
			logprintf(log, LOG_INFO, "Client wants to negotiate TLS\n");
			client_send(log, client, "%s OK Begin TLS negotiation\r\n", sentence.tag);

			client->tls_mode = TLS_NEGOTIATE;

			//this is somewhat dodgy and should probably be replaced by a proper conditional
			return tls_init_serverpeer(log, client, listener_data->tls_priorities, listener_data->tls_cert);
		}

		state = COMMAND_BAD;
	}
	#endif
	else if(!strcasecmp(sentence.command, "authenticate")){
		if(client_data->auth.user.authenticated){
			//this should never happen as successful authentication should transition state
			client_data->state = STATE_AUTHENTICATED;
			state_reason = "Already authenticated";
			state = COMMAND_BAD;
			rv = -1;
		}

		else if((listener_data->auth_offer == AUTH_ANY) ||
				(listener_data->auth_offer == AUTH_TLSONLY && client->tls_mode == TLS_ONLY)){
			if(client_data->auth.auth_tag){
				logprintf(log, LOG_INFO, "Client tried to run multiple AUTHENTICATE commands\n");
				state_reason = "Authentication in progress";
				state = COMMAND_BAD;
				rv = -1;
			}
			else{
				//call out to state_sasl for actual negotiation
				return imapstate_sasl(log, command_queue, sentence, client, database);
			}
		}
		else{
			state_reason = "Authentication not possible now";
			state = COMMAND_BAD;
			rv = -1;
		}
	}
	else if(!strcasecmp(sentence.command, "login")){
		if(client_data->auth.user.authenticated){
			//this should never happen as successful authentication should transition state
			client_data->state = STATE_AUTHENTICATED;
			state_reason = "Already authenticated";
			state = COMMAND_BAD;
			rv = -1;
		}
		else if(!sentence.parameters){
			state_reason = "Parameters expected";
			state = COMMAND_BAD;
			rv = -1;
		}
		else if((listener_data->auth_offer == AUTH_ANY) ||
				(listener_data->auth_offer == AUTH_TLSONLY && client->tls_mode == TLS_ONLY)){

			//read argument astrings
			char* login_user = NULL;
			char* login_password = NULL;
			char* astring_data_end = NULL;

			i = protocol_parse_astring(sentence.parameters, &login_user, &astring_data_end);
			if(i >= 0 && login_user[i]){
				//ensure space between arguments
				if(astring_data_end[0] == ' '){
					//terminate user
					login_user[i] = 0; //this needs to be past the previous condition

					i = protocol_parse_astring(astring_data_end + 1, &login_password, &astring_data_end);
					if(i >= 0){
						//terminate password
						login_password[i] = 0;
					}
					else{
						logprintf(log, LOG_WARNING, "Failed to parse password astring\n");
					}
				}
				else{
					logprintf(log, LOG_WARNING, "Invalid delimiter between arguments: %02X\n", astring_data_end[0]);
				}
			}
			else{
				logprintf(log, LOG_WARNING, "Failed to parse user name astring\n");
			}

			if(login_user && login_password){
				if(auth_validate(log, database, login_user, login_password, &(client_data->auth.user.authorized)) < 0){
					//failed to authenticate
					logprintf(log, LOG_INFO, "Failed to authenticate client\n");
					auth_reset(&(client_data->auth));

					state_reason = "Failed to authenticate";
					state = COMMAND_NO;

					//increase failscore
					rv = -1;
				}
				else{
					client_data->auth.user.authenticated = common_strdup(login_user);
					if(!client_data->auth.user.authenticated){
						logprintf(log, LOG_ERROR, "Failed to allocate memory for authentication user name\n");
						auth_reset(&(client_data->auth));

						state_reason = "Internal error";
						state = COMMAND_BAD;
					}
					else{
						client_data->auth.method = IMAP_LOGIN;
						logprintf(log, LOG_INFO, "Client authenticated as user %s\n", client_data->auth.user.authenticated);

						state = COMMAND_NOREPLY;
						rv = 1;
						//enqueue command to acquire connection specific data
						commandqueue_enqueue_command(log, command_queue, client, sentence, NULL);
					}
				}
			}
			else{
				state_reason = "Bad credentials supplied";
				state = COMMAND_BAD;
				rv = -1;
			}
		}
		else{
			state_reason = "Authentication not possible now";
			state = COMMAND_BAD;
			rv = -1;
		}
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
		case COMMAND_NO:
			client_send(log, client, "%s NO %s\r\n", sentence.tag, state_reason ? state_reason:"Command failed");
			return rv;
		case COMMAND_NOREPLY:
			return rv;
	}

	logprintf(log, LOG_ERROR, "Illegal branch reached in state NEW\n");
	return -2;
}

int imapstate_authenticated(LOGGER log, COMMAND_QUEUE* command_queue, IMAP_COMMAND sentence, CONNECTION* client, DATABASE* database){
	IMAP_COMMAND_STATE state = COMMAND_UNHANDLED;
	char* state_reason = NULL;
	int rv = 0;
	int astring_length;

	//commands valid in any state as per RFC 3501 6.1
	if(!strcasecmp(sentence.command, "capability")){
		state = imap_capability(log, sentence, client, database);
	}
	else if(!strcasecmp(sentence.command, "noop")){
		//this one is easy
		if(commandqueue_enqueue_command(log, command_queue, client, sentence, NULL) < 0){
			logprintf(log, LOG_ERROR, "Failed to enqueue command\n");
			state = COMMAND_BAD;
			state_reason = "Failed to enqueue command";
		}
		else{
			//FIXME maybe wait for queue round-trip instead of direct response here
			state = COMMAND_OK;
		}
	}
	else if(!strcasecmp(sentence.command, "logout")){
		//this is kind of a hack as it bypasses the default command state responder
		return imap_logout(log, sentence, client, database, command_queue);
	}
	else if(!strcasecmp(sentence.command, "xyzzy")){
		//no error handling here, beware of the dragons
		imap_xyzzy(log, sentence, client, database);
		commandqueue_enqueue_command(log, command_queue, client, sentence, NULL);
		state = COMMAND_NOREPLY;
	}

	//AUTHENTICATED state commands, grouped by command signature
	else if(!strcasecmp(sentence.command, "select") || !strcasecmp(sentence.command, "examine")
			|| !strcasecmp(sentence.command, "create") || !strcasecmp(sentence.command, "delete")
			|| !strcasecmp(sentence.command, "subscribe") || !strcasecmp(sentence.command, "unsubscribe")){
		if(!sentence.parameters){
			state = COMMAND_BAD;
			state_reason = "Missing parameter";
			rv = -1;
		}
		else{
			//params mboxname (= astring)
			char* parameters[2] = {NULL, NULL};
			astring_length = protocol_parse_astring(sentence.parameters, parameters, NULL);
			if(astring_length < 0){
				state = COMMAND_BAD;
				state_reason = "Invalid parameter supplied";
				rv = -1;
			}
			else{
				parameters[0][astring_length] = 0;
				if(commandqueue_enqueue_command(log, command_queue, client, sentence, parameters) < 0){
					logprintf(log, LOG_ERROR, "Failed to enqueue command\n");
					state = COMMAND_BAD;
					state_reason = "Failed to enqueue command";
				}
				else{
					//queued command, no direct reply
					state = COMMAND_NOREPLY;
				}
			}
		}
	}
	else if(!strcasecmp(sentence.command, "rename")){
		//TODO params from(astring) to(astring)
	}
	else if(!strcasecmp(sentence.command, "list") || !strcasecmp(sentence.command, "lsub")){
		//TODO params refname(astring) mbxname(astring with %/* allowed)
	}
	else if(!strcasecmp(sentence.command, "status")){
		//TODO params mboxname status-attribs(= paren-list of status-attribs)
	}
	else if(!strcasecmp(sentence.command, "append")){
		//TODO params mboxname [flag-list(paren-list of flags)] [date-time(timefmt)] data(astring)
	}

	//SELECTED state commands, grouped by command signature
	//else if(MBX_SELECTED){
		if(!strcasecmp(sentence.command, "check")){
			//TODO checkpoint the database - fsync?
		}
		else if(!strcasecmp(sentence.command, "close")){
			//TODO close a mailbox
		}
		else if(!strcasecmp(sentence.command, "expunge")){
			//TODO delete flagged messages
		}
		else if(!strcasecmp(sentence.command, "search")){
			//TODO params ["CHARSET" cset(astring)] 1*(search-key)
		}
		else if(!strcasecmp(sentence.command, "fetch")){
			//TODO params seqs(=sequence-sets) fetch-att(=astring) or paren-list of astring
		}
		else if(!strcasecmp(sentence.command, "store")){
			//TODO params lots
		}
		else if(!strcasecmp(sentence.command, "copy")){
			//TODO params seq(=sequence-set) mboxname
		}
		else if(!strcasecmp(sentence.command, "uid")){
			//TODO params lots of weird things
		}
	//}

	switch(state){
		case COMMAND_UNHANDLED:
			logprintf(log, LOG_WARNING, "Unhandled command %s in state AUTHENTICATED\n", sentence.command);
			client_send(log, client, "%s BAD Command not recognized\r\n", sentence.tag); //FIXME is this correct
			return -1;
		case COMMAND_OK:
			client_send(log, client, "%s OK %s\r\n", sentence.tag, state_reason ? state_reason:"Command completed");
			return rv;
		case COMMAND_BAD:
			client_send(log, client, "%s BAD %s\r\n", sentence.tag, state_reason ? state_reason:"Command failed");
			return rv;
		case COMMAND_NO:
			client_send(log, client, "%s NO %s\r\n", sentence.tag, state_reason ? state_reason:"Command failed");
			return rv;
		case COMMAND_NOREPLY:
			return rv;
	}

	logprintf(log, LOG_ERROR, "Illegal branch reached in state AUTHENTICATED\n");
	return -2;
}
