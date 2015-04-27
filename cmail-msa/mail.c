int mail_route(LOGGER log, MAIL* mail, DATABASE* database){
	unsigned i;
	unsigned rv=250;
	struct timespec time_spec;

	//generate a message id
	if(clock_gettime(CLOCK_REALTIME_COARSE, &time_spec)<0){
		logprintf(log, LOG_ERROR, "Failed to get time for message id generation: %s\n", strerror(errno));
		time_spec.tv_sec=time(NULL);
	}

	snprintf(mail->message_id, CMAIL_MESSAGEID_MAX, "%X%X.%X-%s", (unsigned)time_spec.tv_sec, (unsigned)time_spec.tv_nsec, rand(), mail->submitter);
	logprintf(log, LOG_INFO, "Generated message ID %s\n", mail->message_id);

	//iterate over recipients
	for(i=0;mail->forward_paths[i];i++){
		logprintf(log, LOG_DEBUG, "Routing forward path %d: %s (%s)\n", i, mail->forward_paths[i]->path, mail->forward_paths[i]->resolved_user?(mail->forward_paths[i]->resolved_user):"outbound");
		if(mail->forward_paths[i]->resolved_user){
			//inbound mail, apply inrouter
			if(route_inbound(log, database, mail, mail->forward_paths[i])<0){
				logprintf(log, LOG_WARNING, "Failed to route path %s inbound\n", mail->forward_paths[i]->path);
			}
		}
		else{
			//outbound mail, should have been authenticated so accept it automatically
			if(mail_store_outbox(log, database->mail_storage.outbox_master, NULL, mail->forward_paths[i]->path, mail)<0){
				logprintf(log, LOG_WARNING, "Failed to route path %s outbound\n", mail->forward_paths[i]->path);
			}
		}
	}

	return rv;
}

int mail_originate(LOGGER log, char* user, MAIL* mail, DATABASE* database){
	MAILROUTE route;
	int rv=250, i;

	route=route_query(log, database, false, user);

	//user has no routing entry, reject the mail
	if(!route.router){
		return 500;
	}

	logprintf(log, LOG_INFO, "Outbound router for connected user %s is %s (%s)\n", user, route.router, route.argument?route.argument:"none");

	if(!strcmp(route.router, "drop")){
		//done.
	}
	else if(!strcmp(route.router, "handoff")){
		if(route.argument){
			for(i=0;mail->forward_paths[i];i++){
				//insert into outbound table
				logprintf(log, LOG_DEBUG, "Handing off path %d: %s\n", i, mail->forward_paths[i]->path);
				if(mail_store_outbox(log, database->mail_storage.outbox_master, route.argument, mail->forward_paths[i]->path, mail)<0){
					logprintf(log, LOG_WARNING, "Failed to route %s via handoff\n", mail->forward_paths[i]->path);
				}
			}
		}
	}
	else{
		rv=mail_route(log, mail, database);
	}

	route_free(&route);
	return rv;
}

int mail_line(LOGGER log, MAIL* mail, char* line){
	//logprintf(log, LOG_DEBUG, "Mail line is \"%s\"\n", line);
	if(mail->data_max>0 && mail->data_offset >= mail->data_max){
		logprintf(log, LOG_INFO, "Mail length (%d) exceeded data length limit (%d), truncating\n", mail->data_offset, mail->data_max);
		return -1;
	}

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
	logprintf(log, LOG_DEBUG, "Adding %d bytes to mail at index %d\n", strlen(line), mail->data_offset);
	strncpy(mail->data+mail->data_offset, line, strlen(line)+1);
	mail->data_offset+=strlen(line);
	return 0;
}

int mail_recvheader(LOGGER log, MAIL* mail, char* announce){
	char buffer[(SMTP_HEADER_LINE_MAX*4)+1];

	unsigned mark=0, i, off=0;
	int bytes=0;
	time_t unix_time=time(NULL);
	struct tm* local_time=localtime(&unix_time);

	//write received: header
	bytes=snprintf(buffer, sizeof(buffer)-1, "Received: from %s by %s with %s; ", mail->submitter, announce, mail->protocol);

	if(bytes<0){
		return -1;
	}

	if(local_time){
		bytes+=strftime(buffer+bytes, sizeof(buffer)-bytes-1, "%a, %d %b %Y %T %z", local_time);
	}
	else{
		bytes+=snprintf(buffer+bytes, sizeof(buffer)-bytes-1, "Time failed");
	}

	logprintf(log, LOG_DEBUG, "%d bytes of header data: %s\n", bytes, buffer);

	for(i=0;i<=bytes;i++){
		if(buffer[i]==' '){
			mark=i;
		}

		if(((i-off)>=SMTP_HEADER_LINE_MAX && off<mark) || buffer[i]==0){
			//add current contents
			//terminate
			if(buffer[i]){
				buffer[mark]=0;
			}
			mail_line(log, mail, buffer+off);
			//un-terminate
			buffer[mark]='\t';

			off=mark;
			if(buffer[i]==0){
				break;
			}
		}
	}

	return 0;
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
		.data = NULL,
		.submitter = NULL,
		.protocol = NULL,
		.message_id = ""
	};

	if(!mail){
		return -1;
	}

	empty_mail.data_allocated=mail->data_allocated;
	empty_mail.data=mail->data;
	//Keep submitter pointing to the submitter of the CLIENT structure
	empty_mail.submitter=mail->submitter;
	path_reset(&(mail->reverse_path));

	if(mail->data){
		mail->data[0]=0;
	}

	for(i=0;mail->forward_paths[i]&&i<SMTP_MAX_RECIPIENTS;i++){
		pathpool_return(mail->forward_paths[i]);
	}

	*mail=empty_mail;
	return 0;
}

int mail_store_inbox(LOGGER log, sqlite3_stmt* stmt, MAIL* mail, MAILPATH* current_path){
	int status;

	if(sqlite3_bind_text(stmt, 1, current_path->resolved_user, -1, SQLITE_STATIC)!=SQLITE_OK
		|| sqlite3_bind_text(stmt, 2, mail->message_id, -1, SQLITE_STATIC) != SQLITE_OK
		|| sqlite3_bind_text(stmt, 3, current_path->path, -1, SQLITE_STATIC)!=SQLITE_OK
		|| sqlite3_bind_text(stmt, 4, mail->reverse_path.path, -1, SQLITE_STATIC)!=SQLITE_OK
		|| sqlite3_bind_text(stmt, 5, mail->submitter, -1, SQLITE_STATIC)!=SQLITE_OK
		|| sqlite3_bind_text(stmt, 6, mail->protocol, -1, SQLITE_STATIC)!=SQLITE_OK
		|| sqlite3_bind_text(stmt, 7, mail->data, -1, SQLITE_STATIC)!=SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind mail storage parameter\n");
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		return -1;
	}

	status=sqlite3_step(stmt);
	switch(status){
		case SQLITE_DONE:
			status=0;
			break;
		default:
			logprintf(log, LOG_INFO, "Unhandled return value from insert statement: %d\n", status);
			status=1;
	}

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);
	return status;
}

int mail_store_outbox(LOGGER log, sqlite3_stmt* stmt, char* mail_remote, char* envelope_to, MAIL* mail){
	int status;

	if(sqlite3_bind_text(stmt, 1, mail_remote, -1, SQLITE_STATIC)!=SQLITE_OK
		|| sqlite3_bind_text(stmt, 2, mail->reverse_path.path, -1, SQLITE_STATIC)!=SQLITE_OK
		|| sqlite3_bind_text(stmt, 3, envelope_to, -1, SQLITE_STATIC)!=SQLITE_OK
		|| sqlite3_bind_text(stmt, 4, mail->submitter, -1, SQLITE_STATIC)!=SQLITE_OK
		|| sqlite3_bind_text(stmt, 5, mail->data, -1, SQLITE_STATIC)!=SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind mail storage parameter\n");
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		return -1;
	}

	status=sqlite3_step(stmt);
	switch(status){
		case SQLITE_DONE:
			status=0;
			break;
		default:
			logprintf(log, LOG_INFO, "Unhandled return value from insert statement: %d\n", status);
			status=1;
	}

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);
	return status;
}
