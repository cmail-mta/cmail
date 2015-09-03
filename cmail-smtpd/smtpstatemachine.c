int smtpstate_new(LOGGER log, CONNECTION* client, DATABASE* database, PATHPOOL* path_pool){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	LISTENER* listener_data=(LISTENER*)client_data->listener->aux_data;

	if(!strncasecmp(client_data->recv_buffer, "ehlo ", 5)){
		#ifndef CMAIL_NO_TLS
		switch(client_data->listener->tls_mode){
			case TLS_ONLY:
				client_data->current_mail.protocol=(client_data->sasl_user.authenticated?"sesmtpa":"sesmtp");
				break;
			case TLS_NEGOTIATE:
				if(client->tls_mode==TLS_ONLY){
					client_data->current_mail.protocol=(client_data->sasl_user.authenticated?"esmtpsa":"esmtps");
					break;
				}
			case TLS_NONE:
				client_data->current_mail.protocol=(client_data->sasl_user.authenticated?"esmtpa":"esmtp");
				break;
		}
		#else
		client_data->current_mail.protocol=(client_data->sasl_user.authenticated?"esmtpa":"esmtp");
		#endif

		client_data->state=STATE_IDLE;

		client_send(log, client, "250-%s ahoyhoy\r\n", listener_data->announce_domain);

		//TODO hook plugins here

		//send hardcoded esmtp options
		client_send(log, client, "250-SIZE %d\r\n", listener_data->max_size);
		client_send(log, client, "250-8BITMIME\r\n"); //FIXME this might imply more processing than planned
		client_send(log, client, "250-SMTPUTF8\r\n"); //RFC 6531
		switch(listener_data->auth_offer){
			case AUTH_NONE:
				break;
			case AUTH_TLSONLY:
				#ifndef CMAIL_NO_TLS
				if(client->tls_mode!=TLS_ONLY){
					break;
				}
				#else
				break;
				#endif
			case AUTH_ANY:
				client_send(log, client, "250-AUTH PLAIN\r\n");
				break;
		}
		#ifndef CMAIL_NO_TLS
		//advertise only when possible
		if(client_data->listener->tls_mode==TLS_NEGOTIATE && client->tls_mode==TLS_NONE){
			client_send(log, client, "250-STARTTLS\r\n"); //RFC 3207
		}
		#endif
		client_send(log, client, "250 XYZZY\r\n"); //RFC 5321 2.2.2
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "helo ", 5)){
		#ifndef CMAIL_NO_TLS
		switch(client_data->listener->tls_mode){
			case TLS_ONLY:
				client_data->current_mail.protocol = (client_data->sasl_user.authenticated ? "ssmtpa":"ssmtp");
				break;
			case TLS_NEGOTIATE:
				if(client->tls_mode==TLS_ONLY){
					client_data->current_mail.protocol = (client_data->sasl_user.authenticated ? "smtpsa":"smtps");
					break;
				}
			case TLS_NONE:
				client_data->current_mail.protocol = (client_data->sasl_user.authenticated ? "smtpa":"smtp");
				break;
		}
		#else
		client_data->current_mail.protocol = (client_data->sasl_user.authenticated ? "smtpa":"smtp");
		#endif

		client_data->state=STATE_IDLE;
		logprintf(log, LOG_INFO, "Client negotiates smtp\n");

		client_send(log, client, "250 %s ahoyhoy\r\n", listener_data->announce_domain);
		return 0;
	}

	logprintf(log, LOG_INFO, "Command not recognized in state NEW: %s\n", client_data->recv_buffer);
	client_send(log, client, "500 Unknown command\r\n");
	return -1;
}

int smtpstate_auth(LOGGER log, CONNECTION* client, DATABASE* database, PATHPOOL* path_pool){
	CLIENT* client_data = (CLIENT*)client->aux_data;
	int status = SASL_ERROR_PROCESSING;
	char* method = NULL;
	char* parameter = NULL;
	char* challenge = NULL;

	if(client_data->sasl_context.method != SASL_INVALID && !strcmp(client_data->recv_buffer, "*")){
		//cancel authentication
		logprintf(log, LOG_INFO, "Client cancelled authentication\n");
		client_data->state=STATE_IDLE;
		sasl_cancel(&(client_data->sasl_context));
		client_send(log, client, "501 Authentication cancelled\r\n");
		return 0;
	}
	else if(client_data->sasl_context.method != SASL_INVALID){
		status = sasl_continue(log, &(client_data->sasl_context), client_data->recv_buffer, &challenge);
	}
	else if(!strncasecmp(client_data->recv_buffer, "auth ", 5)){
		//tokenize along spaces
		method = strtok(client_data->recv_buffer + 5, " ");
		if(method){
			parameter = strtok(NULL, " ");
			logprintf(log, LOG_DEBUG, "Beginning SASL with method %s parameter %s\n", method, parameter?parameter:"null");
			status = sasl_begin(log, &(client_data->sasl_context), &(client_data->sasl_user), method, parameter, &challenge);
		}
		else{
			logprintf(log, LOG_WARNING, "Client tried auth without supplying method\n");
			status = SASL_UNKNOWN_METHOD;
		}
	}
	else{
		logprintf(log, LOG_ERROR, "Invalid state (No negotiation but data) reached in AUTH, returning to IDLE\r\n");
		client_data->state=STATE_IDLE;
		client_send(log, client, "500 Invalid state\r\n");
		return -1;
	}

	switch(status){
		case SASL_OK:
			logprintf(log, LOG_ERROR, "Invalid state (SASL_OK) reached in AUTH, returning to IDLE\r\n");
			//FIXME might want to reset sasl_user here
			sasl_reset_ctx(&(client_data->sasl_context), false); //opting for security here, rather than freeing memory we do not own
			client_data->state = STATE_IDLE;
			client_send(log, client, "500 Invalid state\r\n");
			return -1;
		case SASL_ERROR_PROCESSING:
			logprintf(log, LOG_ERROR, "SASL processing error\r\n");
			sasl_reset_ctx(&(client_data->sasl_context), true);
			client_send(log, client, "454 Internal Error\r\n");
			client_data->state = STATE_IDLE;
			return 0;
		case SASL_ERROR_DATA:
			logprintf(log, LOG_ERROR, "SASL failed to parse data\r\n");
			sasl_reset_ctx(&(client_data->sasl_context), true);
			client_send(log, client, "501 Invalid data provided\r\n");
			client_data->state = STATE_IDLE;
			return -1;
		case SASL_UNKNOWN_METHOD:
			logprintf(log, LOG_WARNING, "Client tried unsupported authentication method: %s\n", client_data->recv_buffer + 5);
			client_data->state = STATE_IDLE;
			sasl_reset_ctx(&(client_data->sasl_context), false);
			client_send(log, client, "504 Unknown mechanism\r\n");
			return -1;
		case SASL_CONTINUE:
			logprintf(log, LOG_INFO, "Asking for SASL continuation\r\n");
			client_send(log, client, "334 %s\r\n", challenge ? challenge:"");
			return 0;
		case SASL_DATA_OK:
			sasl_reset_ctx(&(client_data->sasl_context), true);
			client_data->state = STATE_IDLE;

			//check auth data
			if(!challenge || auth_validate(log, database, client_data->sasl_user.authenticated, challenge, &(client_data->sasl_user.authorized)) < 0){
				//login failed
				sasl_reset_user(&(client_data->sasl_user), true);
				logprintf(log, LOG_INFO, "Client failed to authenticate\n");
				client_send(log, client, "535 Authentication failed\r\n");

				//increase the failscore
				return -1;
			}

			if(!client_data->sasl_user.authorized){
				sasl_reset_user(&(client_data->sasl_user), true);
				logprintf(log, LOG_ERROR, "Failed to allocate memory for authorized user\n");
				client_send(log, client, "454 Internal Error\r\n");
				return 0;
			}

			client_data->originating_route = route_query(log, database, client_data->sasl_user.authorized);

			logprintf(log, LOG_INFO, "Client authenticated as %s\n", client_data->sasl_user.authenticated);
			client_send(log, client, "235 Authenticated\r\n");

			//update the protocol version
			#ifndef CMAIL_NO_TLS
			switch(client_data->listener->tls_mode){
				case TLS_ONLY:
					client_data->current_mail.protocol = "sesmtpa";
					break;
				case TLS_NEGOTIATE:
					if(client->tls_mode == TLS_ONLY){
						client_data->current_mail.protocol = "esmtpsa";
						break;
					}
				case TLS_NONE:
					client_data->current_mail.protocol = "esmtpa";
					break;
			}
			#else
			client_data->current_mail.protocol = "esmtpa";
			#endif

			//decrease the failscore
			return 1;
	}

	logprintf(log, LOG_ERROR, "Invalid branch reached in AUTH\n");
	return -1;
}

int smtpstate_idle(LOGGER log, CONNECTION* client, DATABASE* database, PATHPOOL* path_pool){
	CLIENT* client_data = (CLIENT*)client->aux_data;
	LISTENER* listener_data = (LISTENER*)client_data->listener->aux_data;

	if(!strncasecmp(client_data->recv_buffer, "noop", 4)){
		logprintf(log, LOG_INFO, "Client noop\n");
		client_send(log, client, "250 OK\r\n");
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "rset", 4)){
		logprintf(log, LOG_INFO, "Client reset\n");
		mail_reset(&(client_data->current_mail));
		client_send(log, client, "250 Reset OK\r\n");
		return 0;
	}

	#ifndef CMAIL_NO_TLS
	//accept only when offered, reject when already negotiated
	if(!strncasecmp(client_data->recv_buffer, "starttls", 8)){
		if(client->tls_mode!=TLS_NONE){
			logprintf(log, LOG_WARNING, "Client with active TLS session tried to negotiate\n");
			client_send(log, client, "503 Already in TLS session\r\n");
			return 0;
		}

		if(client_data->listener->tls_mode!=TLS_NEGOTIATE){
			logprintf(log, LOG_WARNING, "Client tried to negotiate TLS with non-negotiable listener\n");
			client_send(log, client, "503 Not advertised\r\n");

			//increase the failscore
			return -1;
		}

		logprintf(log, LOG_INFO, "Client wants to negotiate TLS\n");
		mail_reset(&(client_data->current_mail));
		client_data->state=STATE_NEW;

		client_send(log, client, "220 Go ahead\r\n");

		client->tls_mode=TLS_NEGOTIATE;

		//this is somewhat dodgy and should probably be replaced by a proper conditional
		return tls_init_serverpeer(log, client, listener_data->tls_priorities, listener_data->tls_cert);
	}
	#endif

	if(!strncasecmp(client_data->recv_buffer, "auth ", 5)){
		switch(listener_data->auth_offer){
			case AUTH_NONE:
				logprintf(log, LOG_WARNING, "Client tried to auth on a non-auth listener\n");
				client_send(log, client, "503 Not advertised\r\n");
				return -1;
			case AUTH_TLSONLY:
				#ifndef CMAIL_NO_TLS
				if(client->tls_mode != TLS_ONLY){
				#endif
					logprintf(log, LOG_WARNING, "Non-TLS client tried to auth on auth-tlsonly listener\n");
					client_send(log, client, "504 Encryption required\r\n"); //FIXME 538 might be better, but is marked obsolete
					return -1;
				#ifndef CMAIL_NO_TLS
				}
				#endif
			case AUTH_ANY:
				if(client_data->sasl_user.authenticated){
					logprintf(log, LOG_WARNING, "Authenticated client tried to authenticate again\n");
					client_send(log, client, "503 Already authenticated\r\n");
					return -1;
				}
				//continue
				break;
		}

		client_data->state = STATE_AUTH;
		return smtpstate_auth(log, client, database, path_pool);
	}

	if(!strncasecmp(client_data->recv_buffer, "xyzzy", 5)){
		client_send(log, client, "250 Nothing happens\r\n");
		logprintf(log, LOG_INFO, "Client performs incantation\n");
		//Using this command for some debug output...
		logprintf(log, LOG_DEBUG, "Client protocol: %s\n", client_data->current_mail.protocol);
		logprintf(log, LOG_DEBUG, "Peer name %s, mail submitter %s, data_allocated %d\n", client_data->peer_name, client_data->current_mail.submitter, client_data->current_mail.data_allocated);
		logprintf(log, LOG_DEBUG, "AUTH Status %s, Authentication: %s, Authorization: %s\n", client_data->sasl_context.method!=SASL_INVALID?"active":"inactive", client_data->sasl_user.authenticated ? client_data->sasl_user.authenticated:"none", client_data->sasl_user.authorized ? client_data->sasl_user.authorized:"none");
		logprintf(log, LOG_DEBUG, "Originating router: %s (%s)\n", client_data->originating_route.router?client_data->originating_route.router:"none", client_data->originating_route.argument?client_data->originating_route.argument:"none");
		#ifndef CMAIL_NO_TLS
		logprintf(log, LOG_DEBUG, "TLS State: %s\n", tls_modestring(client->tls_mode));
		#endif
		logprintf(log, LOG_DEBUG, "Connection score: %d\n", client_data->connection_score);
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "quit", 4)){
		logprintf(log, LOG_INFO, "Client quit\n");
		client_send(log, client, "221 OK Bye\r\n");
		return client_close(client);
	}

	//this needs to be implemented as per RFC5321 3.3
	if(!strncasecmp(client_data->recv_buffer, "rcpt ", 5)){
		logprintf(log, LOG_INFO, "Client tried to use RCPT in IDLE\n");
		client_send(log, client, "503 Bad sequence of commands\r\n");
		return -1;
	}

	if(!strncasecmp(client_data->recv_buffer, "mail from:", 10)){
		if(listener_data->auth_require && !client_data->sasl_user.authenticated){
			logprintf(log, LOG_WARNING, "Client tried mail command without authentication on strict auth listener\n");
			client_send(log, client, "530 Authentication required\r\n");
			return -1;
		}

		logprintf(log, LOG_INFO, "Client initiates mail transaction\n");
		//extract reverse path and store it
		if(path_parse(log, client_data->recv_buffer + 10, &(client_data->current_mail.reverse_path)) < 0){
			client_send(log, client, "501 Path rejected\r\n");
			return -1;
		}

		//resolve reverse path
		switch(path_resolve(log, &(client_data->current_mail.reverse_path), database, client_data->sasl_user.authorized, true)){
			case 0:
				//reverse path contains either store or null router.
				//check origination routing data for action to take
				if(client_data->sasl_user.authenticated && client_data->sasl_user.authorized){
					if(!client_data->originating_route.router || !strcmp(client_data->originating_route.router, "reject")){
						client_send(log, client, "551 %s\r\n", client_data->originating_route.argument ? client_data->originating_route.argument:"User not allowed to use this path");
						path_reset(&(client_data->current_mail.reverse_path));
						return -1;
					}
					else if(!strcmp(client_data->originating_route.router, "any")){
						//accept anything
					}
					else if(!strcmp(client_data->originating_route.router, "defined")){
						if(!client_data->current_mail.reverse_path.route.router
								|| !client_data->current_mail.reverse_path.route.argument
								|| strcmp(client_data->current_mail.reverse_path.route.router, "store")
								|| strcmp(client_data->current_mail.reverse_path.route.argument, client_data->sasl_user.authorized)){
							//no valid store router pointing back at the user, fail the reverse path
							client_send(log, client, "551 User not allowed to use this path\r\n");
							path_reset(&(client_data->current_mail.reverse_path));
							return -1;
						}
						else{
							//the path resolves back to the originating user, accept it
						}
					}
				}
				else{
					//filtering of local reverse paths for unauthenticated connections might take place here
				}
				break;
			case 1:
				//inbound reject router (should not be able to happen anymore)
				logprintf(log, LOG_ERROR, "Inbound reject router applied to reverse path, please notify the developers!\n");
				break;
			default:
				//resolution failed
				logprintf(log, LOG_INFO, "Failed to resolve reverse path\n");
				path_reset(&(client_data->current_mail.reverse_path));
				client_send(log, client, "451 Path rejected\r\n");
				return 0;
		}

		//TODO call plugins for spf, etc

		client_send(log, client, "250 OK\r\n");
		client_data->state = STATE_RECIPIENTS;
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "vrfy ", 5)
			|| !strncasecmp(client_data->recv_buffer, "expn ", 5)){
		logprintf(log, LOG_WARNING, "Client tried to verify / expand an address, unsupported\n");
		client_send(log, client, "502 Not implemented\r\n");
		return -1;
	}

	logprintf(log, LOG_INFO, "Command not recognized in state IDLE: %s\n", client_data->recv_buffer);
	client_send(log, client, "500 Unknown command\r\n");
	return -1;
}

int smtpstate_recipients(LOGGER log, CONNECTION* client, DATABASE* database, PATHPOOL* path_pool){
	CLIENT* client_data = (CLIENT*)client->aux_data;
	LISTENER* listener_data = (LISTENER*)client_data->listener->aux_data;
	unsigned i;
	MAILPATH* current_path;

	if(!strncasecmp(client_data->recv_buffer, "rcpt to:", 8)){
		//get slot in forward_paths
		for(i = 0; i < SMTP_MAX_RECIPIENTS; i++){
			if(!client_data->current_mail.forward_paths[i]){
				break;
			}
		}

		if(i == SMTP_MAX_RECIPIENTS){
			//too many recipients, fail this one
			logprintf(log, LOG_INFO, "Mail exceeded recipient limit\n");
			client_send(log, client, "452 Too many recipients\r\n");
			return -1;
		}

		//get path from pool
		current_path = pathpool_get(log, path_pool);
		if(!current_path){
			logprintf(log, LOG_ERROR, "Failed to get path, failing recipient\n");
			client_send(log, client, "452 Recipients pool maxed out\r\n");
			//FIXME should state transition back to idle here?
			return -1;
		}

		if(path_parse(log, client_data->recv_buffer + 8, current_path) < 0){
			client_send(log, client, "501 Path rejected\r\n");
			pathpool_return(current_path);
			return -1;
		}

		//the last 2 parameters in this call _must_ be NULL/false to not trigger an invalid branch in this case
		switch(path_resolve(log, current_path, database, NULL, false)){
			case 0:
				//continue path handling
				break;
			case 1:
				//reject by router decision
				client_send(log, client, "551 %s\r\n", current_path->route.argument ? current_path->route.argument:"User does not accept mail");
				pathpool_return(current_path);
				return -1;
			default:
				client_send(log, client, "451 Path rejected\r\n");
				pathpool_return(current_path);
				//FIXME should state transition back to idle here?
				//client_data->state=STATE_IDLE;
				//mail_reset(&(client_data->current_mail));
				return 0;
		}

		if(!current_path->route.router){
			//path not local, accept only if authenticated
			if(!client_data->sasl_user.authenticated){
				client_send(log, client, "551 Unknown user\r\n");
				pathpool_return(current_path);
				return -1;
			}
		}

		//FIXME address deduplication?
		//TODO call plugins

		client_data->current_mail.forward_paths[i] = current_path;
		client_send(log, client, "250 Accepted\r\n");

		//decrease the failscore
		return 1;
	}

	if(!strncasecmp(client_data->recv_buffer, "quit", 4)){
		logprintf(log, LOG_INFO, "Client quit\n");
		client_send(log, client, "221 OK Bye\r\n");
		return client_close(client);
	}

	if(!strncasecmp(client_data->recv_buffer, "noop", 4)){
		logprintf(log, LOG_INFO, "Client noop\n");
		client_send(log, client, "250 OK\r\n");
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "rset", 4)){
		client_data->state = STATE_IDLE;
		mail_reset(&(client_data->current_mail));
		logprintf(log, LOG_INFO, "Client reset\n");
		client_send(log, client, "250 Reset OK\r\n");
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "auth", 4)){
		logprintf(log, LOG_INFO, "Client tried to use AUTH in RECIPIENTS\n");
		client_send(log, client, "503 Bad sequence of commands\r\n");
		return -1;
	}

	if(!strncasecmp(client_data->recv_buffer, "data", 4)){
		//reject command if no recipients
		if(!client_data->current_mail.forward_paths[0]){
			logprintf(log, LOG_INFO, "Data without recipients\n");
			client_send(log, client, "503 No valid recipients\r\n");
			return -1;
		}
		client_data->state = STATE_DATA;

		//write received: header
		if(mail_recvheader(log, &(client_data->current_mail), listener_data->announce_domain) < 0){
			logprintf(log, LOG_WARNING, "Failed to write received header\n");
		}

		logprintf(log, LOG_INFO, "Client begins data transmission\n");

		client_send(log, client, "354 Go ahead\r\n");
		return 0;
	}

	logprintf(log, LOG_INFO, "Command not recognized in state RECIPIENTS: %s\n", client_data->recv_buffer);
	client_send(log, client, "500 Unknown command\r\n");
	return -1;
}

int smtpstate_data(LOGGER log, CONNECTION* client, DATABASE* database, PATHPOOL* path_pool){
	CLIENT* client_data = (CLIENT*)client->aux_data;

	if(client_data->recv_buffer[0] == '.'){
		if(client_data->recv_buffer[1]){
			logprintf(log, LOG_INFO, "Data line with leading dot, fixing\n");
			//skip leading dot
			//FIXME use return value (might indicate message too long)
			if(mail_line(log, &(client_data->current_mail), client_data->recv_buffer+1) < 0){
				logprintf(log, LOG_WARNING, "Failed to store mail data line\n");
			}
			return 0;
		}
		else{
			//end of mail data signalled
			//check if mail was too big
			if(client_data->current_mail.data_max && client_data->current_mail.data_offset > client_data->current_mail.data_max){
				logprintf(log, LOG_WARNING, "End of mail, data section exceeded size limitation, rejecting\n");
				client_send(log, client, "552 Size limit exceeded\r\n");
			}
			//check for too many hops
			else if(client_data->current_mail.hop_count > CMAIL_MAX_HOPS){
				logprintf(log, LOG_WARNING, "Mail exceeded maximum hop count of %d", CMAIL_MAX_HOPS);
				client_send(log, client, "554 Too many hops\r\n");
			}
			else{
				logprintf(log, LOG_INFO, "End of mail data, routing\n");

				//TODO call plugins here
				if(!client_data->sasl_user.authenticated){
					switch(mail_route(log, &(client_data->current_mail), database)){
						case 250:
							logprintf(log, LOG_INFO, "Incoming mail accepted from %s\n", client_data->peer_name);
							client_send(log, client, "250 OK\r\n");
							break;
						case 400:
							logprintf(log, LOG_INFO, "Temporary routing failure, deferring message\n");
							client_send(log, client, "451 Temporary failure, please try again later. If the error persists, contact the administrator.\r\n");
							break;
						default:
							logprintf(log, LOG_WARNING, "Mail not routed, rejecting\n");
							client_send(log, client, "554 Rejected\r\n");
							break;
					}
				}
				else{
					switch(mail_originate(log, client_data->sasl_user.authorized, &(client_data->current_mail), client_data->originating_route, database)){
						case 250:
							logprintf(log, LOG_INFO, "Originating mail accepted for user %s (auth %s) from %s\n", client_data->sasl_user.authorized, client_data->sasl_user.authenticated, client_data->peer_name);
							client_send(log, client, "250 OK\r\n");
							break;
						case 400:
							logprintf(log, LOG_INFO, "Temporary routing failure, deferring message\n");
							client_send(log, client, "451 Temporary failure, please try again later. If the error persists, contact the administrator.\r\n");
							break;
						default:
							logprintf(log, LOG_WARNING, "Originated mail could not be routed, rejecting\n");
							client_send(log, client, "554 Rejected\r\n");
							break;
					}
				}
			}
			mail_reset(&(client_data->current_mail));
			client_data->state=STATE_IDLE;
		}
	}
	else{
		//detect end of header & count hops
		if(client_data->current_mail.header_offset == 0){
			//header end
			if(client_data->recv_buffer[0] == 0){
				client_data->current_mail.header_offset = client_data->current_mail.data_offset;
				logprintf(log, LOG_DEBUG, "End of header detected at %d\n", client_data->current_mail.header_offset);
			}
			//count Received: headers
			else if(!strncmp(client_data->recv_buffer, "Received: ", 10)){
				client_data->current_mail.hop_count++;
				logprintf(log, LOG_DEBUG, "Mail is now at %d hops\n", client_data->current_mail.hop_count);
			}
		}

		//FIXME use return value (might indicate message too long)
		if(mail_line(log, &(client_data->current_mail), client_data->recv_buffer) < 0){
			logprintf(log, LOG_WARNING, "Failed to store mail data line\n");
		}
		return 0;
	}

	return 0;
}
