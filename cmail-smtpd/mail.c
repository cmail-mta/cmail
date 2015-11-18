int mail_route(LOGGER log, MAIL* mail, DATABASE* database){
	unsigned i;
	unsigned rv = 250;
	struct timespec time_spec;

	//generate a message id
	if(clock_gettime(CLOCK_REALTIME_COARSE, &time_spec) < 0){
		logprintf(log, LOG_ERROR, "Failed to get time for message id generation: %s\n", strerror(errno));
		time_spec.tv_sec = time(NULL);
	}

	snprintf(mail->message_id, CMAIL_MESSAGEID_MAX, "%X%X.%X-%s", (unsigned)time_spec.tv_sec, (unsigned)time_spec.tv_nsec, rand(), mail->submitter);
	logprintf(log, LOG_INFO, "Generated message ID %s\n", mail->message_id);

	//iterate over recipients
	for(i = 0; mail->forward_paths[i]; i++){
		logprintf(log, LOG_DEBUG, "Routing forward path %d: %s (%s)\n", i, mail->forward_paths[i]->path, mail->forward_paths[i]->route.router ? (mail->forward_paths[i]->route.router):"outbound");
		if(mail->forward_paths[i]->route.router){
			//inbound mail, apply inrouter
			//TODO RFC5321 4.4 (P59) says partial accepted recipient list should 200 and send failure notifications
			switch(route_local_path(log, database, mail, mail->forward_paths[i])){
				case 0:
					break;
				case 1:
					logprintf(log, LOG_WARNING, "Failed to store inbound entry for path %d (%s), deferring transaction\n", i, mail->forward_paths[i]->path);
					return 400;
				default:
					logprintf(log, LOG_WARNING, "Failed to route path %d (%s) inbound, rejecting transaction\n", i, mail->forward_paths[i]->path);
					return 500;
			}
		}
		else{
			//outbound mail, should have been authenticated so accept it automatically
			switch(mail_store_outbox(log, database->mail_storage.outbox_master, NULL, mail->forward_paths[i]->path, mail)){
				case 0:
					break;
				case 1:
					logprintf(log, LOG_WARNING, "Failed to store outbound entry for path %d (%s), deferring transaction\n", i, mail->forward_paths[i]->path);
					return 400;
				default:
					logprintf(log, LOG_WARNING, "Failed to route path %d (%s) outbound, rejecting transaction\n", i, mail->forward_paths[i]->path);
					return 500;
			}
		}
	}

	return rv;
}

int mail_originate(LOGGER log, char* user, MAIL* mail, MAILROUTE route, DATABASE* database){
	int rv = 250, i;

	//user has no routing entry, reject the mail
	//this should already have happened early in the conversation
	if(!route.router){
		return 500;
	}

	logprintf(log, LOG_INFO, "Outbound router for connected user %s is %s (%s)\n", user, route.router, route.argument?route.argument:"none");

	if(!strcmp(route.router, "drop")){
		//done.
	}
	else if(!strcmp(route.router, "handoff")){
		if(route.argument){
			for(i = 0; mail->forward_paths[i]; i++){
				//insert into outbound table
				logprintf(log, LOG_DEBUG, "Handing off path %d: %s\n", i, mail->forward_paths[i]->path);
				switch(mail_store_outbox(log, database->mail_storage.outbox_master, route.argument, mail->forward_paths[i]->path, mail)){
					case 0:
						break;
					case 1:
						logprintf(log, LOG_WARNING, "Failed to store handoff entry for path %d (%s), deferring transaction\n", i, mail->forward_paths[i]->path);
						return 400;
					default:
						logprintf(log, LOG_WARNING, "Failed to route path %d (%s) via handoff, rejecting transaction\n", i, mail->forward_paths[i]->path);
						return 500;
				}
			}
		}
	}
	else{
		rv = mail_route(log, mail, database);
	}

	//may check for invalid routers here

	return rv;
}

int mail_line(LOGGER log, MAIL* mail, char* line){
	//logprintf(log, LOG_DEBUG, "Mail line is \"%s\"\n", line);
	if(mail->data_max>0 && mail->data_offset >= mail->data_max){
		logprintf(log, LOG_INFO, "Mail length (%d) exceeded data length limit (%d), truncating\n", mail->data_offset, mail->data_max);
		return -1;
	}

	if(!mail->data || mail->data_allocated < mail->data_offset+strlen(line) + 3){
		mail->data = realloc(mail->data, mail->data_allocated + strlen(line) + 3);
		if(!mail->data){
			logprintf(log, LOG_ERROR, "Failed to reallocate mail buffer\n");
			return -1;
		}
		mail->data_allocated += strlen(line) + 3;
		logprintf(log, LOG_DEBUG, "Reallocated mail data buffer to %d bytes\n", mail->data_allocated);
	}

	if(mail->data_offset != 0){
		//insert crlf
		logprintf(log, LOG_DEBUG, "Inserting newline into data buffer\n");
		mail->data[mail->data_offset++] = '\r';
		mail->data[mail->data_offset++] = '\n';
	}

	//mind the terminator
	logprintf(log, LOG_DEBUG, "Adding %d bytes to mail at index %d\n", strlen(line), mail->data_offset);
	strncpy(mail->data + mail->data_offset, line, strlen(line) + 1);
	mail->data_offset += strlen(line);
	return 0;
}

int mail_recvheader(LOGGER log, MAIL* mail, char* announce, bool suppress_submitter){
	char time_buffer[SMTP_HEADER_LINE_MAX];
	char* recv_header = NULL;
	unsigned header_allocated = 0;

	unsigned mark=0, i, off=0;

	MAILPATH* forward_path = (mail->forward_paths[1]) ? NULL:mail->forward_paths[0];

	//create timestring
	if(common_tprintf("%a, %d %b %Y %T %z", time(NULL), time_buffer, sizeof(time_buffer) - 1) < 0){
		logprintf(log, LOG_ERROR, "Failed to get current time, buffer length probably not suitable\n");
		snprintf(time_buffer, sizeof(time_buffer)-1, "Time failed");
	}

	//write received: header
	recv_header = common_strappf(recv_header, &header_allocated,
			"Received: from %s by %s%s%s with %s; %s",
			suppress_submitter ? "remote":mail->submitter,
			announce,
			forward_path ? " for ":"",
			forward_path ? forward_path->path:"",
			mail->protocol,
			time_buffer);

	if(!recv_header){
		logprintf(log, LOG_ERROR, "Failed to allocate memory for Received: header\n");
		return -1;
	}

	logprintf(log, LOG_DEBUG, "%d bytes of header data: %s\n", header_allocated, recv_header);

	for(i = 0; i <= header_allocated; i++){
		if(recv_header[i] == ' '){
			mark = i;
		}

		if(((i - off) >= SMTP_HEADER_LINE_MAX && off < mark) || recv_header[i] == 0){
			//add current contents
			//terminate
			if(recv_header[i]){
				recv_header[mark] = 0;
			}
			mail_line(log, mail, recv_header + off);
			//un-terminate
			recv_header[mark] = ' ';

			off = mark;
			if(recv_header[i] == 0){
				break;
			}
		}
	}

	free(recv_header);
	return 0;
}

int mail_reset(MAIL* mail){
	unsigned i;

	if(!mail){
		return -1;
	}

	//changes made here must be reflected in client_accept
	MAIL empty_mail = {
		.reverse_path = {
			.delimiter_position = 0,
			.in_transaction = false,
			.path = "",
			.route = {
				.router = NULL,
				.argument = NULL
			}
		},
		.forward_paths = {
			NULL
		},
		.data_offset = 0,
		.data_allocated = 0,
		.data = NULL,
		.submitter = NULL,
		//FIXME this might pose a security risk, setting to UNKNOWN might be better.
		//Setting to NULL breaks the constraint when inserting
		.protocol = mail->protocol,
		.message_id = "",
		.hop_count = 0,
		.header_offset = 0
	};

	empty_mail.data_allocated = mail->data_allocated;
	empty_mail.data = mail->data;
	//Keep submitter pointing to the submitter of the CLIENT structure
	empty_mail.submitter = mail->submitter;
	path_reset(&(mail->reverse_path));

	if(mail->data){
		mail->data[0] = 0;
	}

	for(i = 0; i < SMTP_MAX_RECIPIENTS && mail->forward_paths[i]; i++){
		pathpool_return(mail->forward_paths[i]);
	}

	*mail = empty_mail;
	return 0;
}

int mail_store_inbox(LOGGER log, sqlite3_stmt* stmt, MAIL* mail, MAILPATH* current_path){
	//calling contract: 0 -> ok, -1 -> fail, 1 -> defer
	int status;

	if(sqlite3_bind_text(stmt, 1, current_path->route.argument, -1, SQLITE_STATIC) != SQLITE_OK
		|| sqlite3_bind_text(stmt, 2, mail->message_id, -1, SQLITE_STATIC) != SQLITE_OK
		|| sqlite3_bind_text(stmt, 3, current_path->path, -1, SQLITE_STATIC) != SQLITE_OK
		|| sqlite3_bind_text(stmt, 4, mail->reverse_path.path, -1, SQLITE_STATIC) != SQLITE_OK
		|| sqlite3_bind_text(stmt, 5, mail->submitter, -1, SQLITE_STATIC) != SQLITE_OK
		|| sqlite3_bind_text(stmt, 6, mail->protocol, -1, SQLITE_STATIC) != SQLITE_OK
		|| sqlite3_bind_text(stmt, 7, mail->data, -1, SQLITE_STATIC) != SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind mail storage parameter\n");
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		return -1;
	}

	status = sqlite3_step(stmt);
	switch(status){
		case SQLITE_DONE:
			status = 0;
			break;
		case SQLITE_TOOBIG:
		case SQLITE_CONSTRAINT:
		case SQLITE_RANGE:
			status = -1;
			break;
		default:
			logprintf(log, LOG_INFO, "Unhandled return value from insert statement: %d\n", status);
			status = 1;
	}

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);
	return status;
}

int mail_store_outbox(LOGGER log, sqlite3_stmt* stmt, char* mail_remote, char* envelope_to, MAIL* mail){
	//calling contract: 0 -> ok, -1 -> fail, 1 -> defer
	int status;

	if(sqlite3_bind_text(stmt, 1, mail_remote, -1, SQLITE_STATIC) != SQLITE_OK
		|| sqlite3_bind_text(stmt, 2, mail->reverse_path.path, -1, SQLITE_STATIC) != SQLITE_OK
		|| sqlite3_bind_text(stmt, 3, envelope_to, -1, SQLITE_STATIC) != SQLITE_OK
		|| sqlite3_bind_text(stmt, 4, mail->submitter, -1, SQLITE_STATIC) != SQLITE_OK
		|| sqlite3_bind_text(stmt, 5, mail->data, -1, SQLITE_STATIC) != SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind mail storage parameter\n");
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		return -1;
	}

	status = sqlite3_step(stmt);
	switch(status){
		case SQLITE_DONE:
			status = 0;
			break;
		case SQLITE_TOOBIG:
		case SQLITE_CONSTRAINT:
		case SQLITE_RANGE:
			status = -1;
			break;
		default:
			logprintf(log, LOG_INFO, "Unhandled return value from insert statement: %d\n", status);
			status = 1;
			break;
	}

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);
	return status;
}
