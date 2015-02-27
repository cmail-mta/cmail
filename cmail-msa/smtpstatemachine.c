/*
 * Transitions
 * 	NEW | HELO/EHLO -> IDLE | 250 CAPS
 * 	NEW | *		-> NEW | 500 Out of sequence
 *	IDLE | RCPT	-> MAIL | 250 OK
 *	IDLE | RSET	-> IDLE | 250 OK
 *	MAIL | RSET	-> IDLE | 250 OK
 */

int smtpstate_new(LOGGER log, CONNECTION* client, sqlite3* master){
	//TODO
	return 0;
}

int smtpstate_idle(LOGGER log, CONNECTION* client, sqlite3* master){
	//TODO
	return 0;
}

int smtpstate_mail(LOGGER log, CONNECTION* client, sqlite3* master){
	//TODO
	return 0;
}
