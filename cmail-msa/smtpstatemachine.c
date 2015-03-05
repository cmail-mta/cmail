/*
 * Transitions
 * 	NEW | HELO/EHLO -> IDLE | 250 CAPS
 * 	NEW | *		-> NEW | 500 Out of sequence
 *	IDLE | MAIL	-> MAIL | 250 OK
 *	IDLE | RSET	-> IDLE | 250 OK
 *	MAIL | RSET	-> IDLE | 250 OK
 */

int smtpstate_new(LOGGER log, CONNECTION* client, sqlite3* master, PATHPOOL* path_pool){
	CLIENT* client_data=(CLIENT*)client->aux_data;

	if(!strncasecmp(client_data->recv_buffer, "ehlo ", 5)){
		client_data->state=STATE_IDLE;
		logprintf(log, LOG_INFO, "Client negotiates esmtp\n");

		send(client->fd, "250-", 4, 0);
		send(client->fd, ((LISTENER*)client_data->listener->aux_data)->announce_domain, strlen(((LISTENER*)client_data->listener->aux_data)->announce_domain), 0);
		send(client->fd, " welcomes you\r\n", 16, 0);
		
		//TODO hook plugins here
		
		//send hardcoded esmtp options
		send(client->fd, "250-SIZE 52428800\r\n", 19, 0); //FIXME make this configurable
		send(client->fd, "250-8BITMIME\r\n", 14, 0); //FIXME this might provide some fun down the road...
		send(client->fd, "250 XYZZY\r\n", 11, 0); //RFC 5321 2.2.2
		return 0;
	}
	
	if(!strncasecmp(client_data->recv_buffer, "helo ", 5)){
		client_data->state=STATE_IDLE;
		logprintf(log, LOG_INFO, "Client negotiates smtp\n");

		send(client->fd, "250 ", 4, 0);
		send(client->fd, ((LISTENER*)client_data->listener->aux_data)->announce_domain, strlen(((LISTENER*)client_data->listener->aux_data)->announce_domain), 0);
		send(client->fd, " at your service\r\n", 18, 0);
		return 0;
	}
	
	logprintf(log, LOG_INFO, "Command not recognized in state NEW: %s\n", client_data->recv_buffer);
	send(client->fd, "500 Unknown command\r\n", 21, 0);
	return -1;		
}

int smtpstate_idle(LOGGER log, CONNECTION* client, sqlite3* master, PATHPOOL* path_pool){
	CLIENT* client_data=(CLIENT*)client->aux_data;

	if(!strncasecmp(client_data->recv_buffer, "noop", 4)
			|| !strncasecmp(client_data->recv_buffer, "rset", 4)){
		logprintf(log, LOG_INFO, "Client noop\n");
		send(client->fd, "250 OK\r\n", 8, 0);
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "xyzzy", 5)){
		send(client->fd, "250 Nothing happens\r\n", 21, 0);
		logprintf(log, LOG_INFO, "Client performs incantation\n");

		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "quit", 4)){
		logprintf(log, LOG_INFO, "Client quit\n");
		send(client->fd, "221 OK Bye\r\n", 12, 0);
		return client_close(client);
	}

	//this needs to be implemented as per RFC5321 3.3
	if(!strncasecmp(client_data->recv_buffer, "rcpt ", 5)){
		logprintf(log, LOG_INFO, "Client tried to use RCPT in IDLE\n");
		send(client->fd, "503 Bad sequence of commands\r\n", 30, 0);
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "mail from:", 10)){
		logprintf(log, LOG_INFO, "Client initiates mail transaction\n");
		//extract reverse path and store it
		if(path_parse(log, client_data->recv_buffer+10, &(client_data->current_mail.reverse_path))<0){
			//TODO send malformed address
			return -1;
		}
		//TODO check sending address
		//TODO call plugins
		send(client->fd, "250 OK\r\n", 8, 0);
		client_data->state=STATE_RECIPIENTS;
		return 0;
	}

	logprintf(log, LOG_INFO, "Command not recognized in state IDLE: %s\n", client_data->recv_buffer);
	send(client->fd, "500 Unknown command\r\n", 21, 0);
	return -1;
}

int smtpstate_recipients(LOGGER log, CONNECTION* client, sqlite3* master, PATHPOOL* path_pool){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	unsigned i;
	MAILPATH* current_path;

	if(!strncasecmp(client_data->recv_buffer, "rcpt to:", 8)){
		//get slot in forward_paths
		for(i=0;i<SMTP_MAX_RECIPIENTS;i++){
			if(!client_data->current_mail.forward_paths[i]){
				break;
			}
		}

		if(i==SMTP_MAX_RECIPIENTS){
			//to many recipients, fail this one
			logprintf(log, LOG_INFO, "Mail exceeded recipient limit\n");
			send(client->fd, "550 Too many recipients\r\n", 25, 0); //FIXME correct message
			return 0;
		}

		//get path from pool
		current_path=pathpool_get(log, path_pool);
		if(!current_path){
			logprintf(log, LOG_ERROR, "Failed to get path, failing recipient\n");
			send(client->fd, "420 Recipients pool maxed out\r\n", 31, 0); //FIXME correct message
			return 0;
		}

		if(path_parse(log, client_data->recv_buffer+8, current_path)<0){
			//TODO send malformed address
			return -1;
		}
		//TODO resolve path
		//TODO of not resolved, reject
		//TODO check if already in recipients list
		//if yes, accept but dont store
		//TODO call plugins

		client_data->current_mail.forward_paths[i]=current_path;
		send(client->fd, "250 Accepted\r\n", 14, 0);
		return 0;
	}
	
	if(!strncasecmp(client_data->recv_buffer, "quit", 4)){
		logprintf(log, LOG_INFO, "Client quit\n");
		send(client->fd, "221 OK Bye\r\n", 12, 0);
		return client_close(client);
	}

	if(!strncasecmp(client_data->recv_buffer, "noop", 4)){
		logprintf(log, LOG_INFO, "Client noop\n");
		send(client->fd, "250 OK\r\n", 8, 0);
		return 0;
	}
	
	if(!strncasecmp(client_data->recv_buffer, "rset", 4)){
		client_data->state=STATE_IDLE;
		//reset forward/reverse paths
		mail_reset(&(client_data->current_mail));
		logprintf(log, LOG_INFO, "Client reset\n");
		send(client->fd, "250 OK\r\n", 8, 0);
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "data", 4)){
		client_data->state=STATE_DATA;
		logprintf(log, LOG_INFO, "Client wants to begin data transmission\n");
		send(client->fd, "354 Go ahead\r\n", 14, 0);
		return 0;
	}

	logprintf(log, LOG_INFO, "Command not recognized in state RECIPIENTS: %s\n", client_data->recv_buffer);
	send(client->fd, "500 Unknown command\r\n", 21, 0);
	return -1;
}

int smtpstate_data(LOGGER log, CONNECTION* client, sqlite3* master, PATHPOOL* path_pool){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	
	if(client_data->recv_buffer[0]=='.'){
		if(client_data->recv_buffer[1]){
			//skip leading dot
			//FIXME use return value (might indicate message too long)
			return mail_line(log, &(client_data->current_mail), client_data->recv_buffer+1);
		}
		else{
			//end of mail
			mail_route(log, &(client_data->current_mail), master);
			mail_reset(&(client_data->current_mail));
			client_data->state=STATE_IDLE;
		}
	}
	else{
		//FIXME use return value (might indicate message too long)
		return mail_line(log, &(client_data->current_mail), client_data->recv_buffer+1);
	}

	return 0;
}
