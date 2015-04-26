int mail_dbread(LOGGER log, MAIL* mail, sqlite3_stmt* stmt){
	return -1;
}

int mail_dispatch(LOGGER log, DATABASE* database, MAIL* mail){
	return -1;
}

int mail_free(MAIL* mail){
	unsigned i;
	MAIL empty_mail = {
		.ids = NULL,
		.remote = NULL,
		.mailhost = NULL,
		.recipients = NULL,
		.length = 0,
		.data = NULL
	};

	if(mail->ids){
		free(mail->ids);
	}

	if(mail->remote){
		free(mail->remote);
	}

	if(mail->mailhost){
		free(mail->mailhost);
	}

	if(mail->recipients){
		for(i=0;mail->recipients[i];i++){
			free(mail->recipients[i]);
		}
		free(mail->recipients);
	}

	*mail=empty_mail;
	return 0;
}
