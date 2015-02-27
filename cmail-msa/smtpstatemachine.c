/*
 * Transitions
 * 	NEW | HELO/EHLO -> IDLE | 250 CAPS
 * 	NEW | *		-> NEW | 500 Out of sequence
 *	IDLE | RCPT	-> MAIL | 250 OK
 *	IDLE | RSET	-> IDLE | 250 OK
 *	MAIL | RSET	-> IDLE | 250 OK
 */

int smtpstate_new(LOGGER log, CONNECTION* client, sqlite3* master){
	CLIENT* client_data=(CLIENT*)client->aux_data;

	if(!strncasecmp(client_data->recv_buffer, "ehlo ", 5)){
		client_data->state=STATE_IDLE;

		send(client->fd, "250 ", 4, 0);
		send(client->fd, ((LISTENER*)client_data->listener->aux_data)->announce_domain, strlen(((LISTENER*)client_data->listener->aux_data)->announce_domain), 0);
		send(client->fd, " welcomes you\r\n", 16, 0);
		return 0;
	}
	else if(!strncasecmp(client_data->recv_buffer, "helo ", 5)){
		client_data->state=STATE_IDLE;

		send(client->fd, "250 ", 4, 0);
		send(client->fd, ((LISTENER*)client_data->listener->aux_data)->announce_domain, strlen(((LISTENER*)client_data->listener->aux_data)->announce_domain), 0);
		send(client->fd, " welcomes you\r\n", 16, 0);
		return 0;
	}
	
	send(client->fd, "500 Unknown command\r\n", 21, 0);
	return -1;		
}

int smtpstate_idle(LOGGER log, CONNECTION* client, sqlite3* master){
	//TODO
	return 0;
}

int smtpstate_mail(LOGGER log, CONNECTION* client, sqlite3* master){
	//TODO
	return 0;
}
