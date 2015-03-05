int mail_route(LOGGER log, MAIL* mail, sqlite3* master){
	//TODO
	unsigned i;
	logprintf(log, LOG_INFO, "Now routing mail from %s\n", mail->reverse_path.path);
	for(i=0;mail->forward_paths[i];i++){
		//FIXME second check should not be needed, unmatched paths should not be accepted
		logprintf(log, LOG_INFO, "Forward route to %s (Resolved to %s)\n", mail->forward_paths[i]->path, (mail->forward_paths[i]->resolved_user)?mail->forward_paths[i]->resolved_user:"null");
	}
	if(mail->data){
		logprintf(log, LOG_INFO, "%s\n", mail->data);
	}
	else{
		logprintf(log, LOG_INFO, "No data\n");
	}

	return 0;
}

int mail_line(LOGGER log, MAIL* mail, char* line){
	//TODO
	return -1;
}

int mail_reset(MAIL* mail){
	unsigned i;

	//changes made here must be reflected in client_accept
	MAIL empty_mail = {
		.reverse_path = {
			.in_transaction = false,
			.path = "",
			.resolved_user = NULL
		},
		.forward_paths = {
			NULL
		},
		.data_allocated = 0,
		.data = NULL
	};

	if(!mail){
		return -1;
	}

	empty_mail.data_allocated=mail->data_allocated;
	empty_mail.data=mail->data;

	if(mail->data){
		mail->data[0]=0;
	}

	for(i=0;mail->forward_paths[i]&&i<SMTP_MAX_RECIPIENTS;i++){
		pathpool_return(mail->forward_paths[i]);
	}

	*mail=empty_mail;
	return 0;
}
