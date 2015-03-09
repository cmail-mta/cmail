int mail_route(LOGGER log, MAIL* mail, DATABASE database){
	//TODO
	unsigned i;
	logprintf(log, LOG_INFO, "Now routing mail from %s\n", mail->reverse_path.path);
	for(i=0;mail->forward_paths[i];i++){
		//FIXME second check should not be needed, unmatched paths should not be accepted
		//UPDATE yes they are in relay operation
		logprintf(log, LOG_INFO, "Forward route to %s (Resolved to %s)\n", mail->forward_paths[i]->path, (mail->forward_paths[i]->resolved_user)?mail->forward_paths[i]->resolved_user:"null");
	}
	if(mail->data){
		logprintf(log, LOG_INFO, "%s\n", mail->data);
	}
	else{
		logprintf(log, LOG_INFO, "No data\n");
	}

	return 250;
}

int mail_line(LOGGER log, MAIL* mail, char* line){
	logprintf(log, LOG_DEBUG, "Mail line is \"%s\"\n", line);
	//FIXME check for max line length
	if(!mail->data || mail->data_allocated < mail->data_offset+strlen(line)+3){
		mail->data=realloc(mail->data, mail->data_allocated+strlen(line)+3);
		if(!mail->data){
			logprintf(log, LOG_ERROR, "Failed to reallocate mail buffer\n");
			return -1;
		}
		mail->data_allocated+=strlen(line)+3;
		logprintf(log, LOG_DEBUG, "Reallocated mail data buffer to %d bytes\n", mail->data_allocated);
	}

	if(mail->data_offset!=0){
		//insert crlf
		logprintf(log, LOG_DEBUG, "Inserting newline into data buffer\n");
		mail->data[mail->data_offset++]='\r';
		mail->data[mail->data_offset++]='\n';
	}

	//mind the terminator
	logprintf(log, LOG_DEBUG, "Copying %d bytes to index %d\n", strlen(line), mail->data_offset);
	strncpy(mail->data+mail->data_offset, line, strlen(line)+1);
	mail->data_offset+=strlen(line);
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
		.data_offset = 0,
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
