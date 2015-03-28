int pop_capa(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	LISTENER* listener_data=(LISTENER*)client_data->listener->aux_data;

	logprintf(log, LOG_DEBUG, "Client requested capability listing\n");

	client_send(log, client, "+OK Capability listing\r\n");
	
	//allow tls-only auth
	#ifndef CMAIL_NO_TLS
	if(client_data->tls_mode == TLS_ONLY || !listener_data->tls_require){
	#endif
	client_send(log, client, "USER\r\n");
	client_send(log, client, "SASL LOGIN\r\n");
	#ifndef CMAIL_NO_TLS
	}
	#endif

	#ifndef CMAIL_NO_TLS
	//do not announce when already in tls or no tls possible
	if(client_data->tls_mode == TLS_NONE && listener_data->tls_mode == TLS_NEGOTIATE){
		client_send(log, client, "STLS\r\n");
	}
	#endif

	client_send(log, client, "IMPLEMENTATION %s\r\n", VERSION);
	client_send(log, client, "XYZZY\r\n", VERSION);
	client_send(log, client, ".\r\n", VERSION);

	return 0;
}

int pop_stat(LOGGER log, CONNECTION* client, DATABASE* database){
	unsigned maildrop_bytes=0, i;
	CLIENT* client_data=(CLIENT*)client->aux_data;
	
	//calculate maildrop size
	for(i=0;i<client_data->maildrop.count;i++){
		maildrop_bytes+=client_data->maildrop.mails[i].mail_size;
	}
	
	client_send(log, client, "+OK %d %d\r\n", client_data->maildrop.count, maildrop_bytes);
	return 0;
}

int pop_list(LOGGER log, CONNECTION* client, DATABASE* database, unsigned mail){
	unsigned i;
	CLIENT* client_data=(CLIENT*)client->aux_data;

	if(mail==0){
		//list all mail, multiline
		client_send(log, client, "+OK Scan listing follows\r\n");
		for(i=0;i<client_data->maildrop.count;i++){
			if(!client_data->maildrop.mails[i].flag_delete){
				client_send(log, client, "%d %d\r\n", i+1, client_data->maildrop.mails[i].mail_size);
			}
		}
		client_send(log, client, ".\r\n");
	}
	else if(mail<=client_data->maildrop.count){
		if(client_data->maildrop.mails[mail-1].flag_delete){
			client_send(log, client, "-ERR Mail marked for deletion\r\n");
			return -1;
		}
		else{
			client_send(log, client, "+OK %d %d\r\n", mail, client_data->maildrop.mails[mail-1].mail_size);
		}
	}
	else{
		client_send(log, client, "-ERR No such mail\r\n");
		return -1;
	}

	return 0;
}

int pop_retr(LOGGER log, CONNECTION* client, DATABASE* database, unsigned mail){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	sqlite3_stmt* fetch_stmt;
	char* mail_data=NULL;
	char* mail_bytestuff=NULL;

	if(mail == 0 || mail>client_data->maildrop.count){
		client_send(log, client, "-ERR No such mail\r\n");
		return -1;
	}
	else if(client_data->maildrop.mails[mail-1].flag_delete){
		client_send(log, client, "-ERR Mail marked for deletion\r\n");
		return -1;
	}

	if(client_data->maildrop.mails[mail-1].flag_master){
		fetch_stmt=database->fetch_master;
	}
	else{
		fetch_stmt=client_data->maildrop.fetch_user;
	}

	if(sqlite3_bind_int(fetch_stmt, 1, client_data->maildrop.mails[mail-1].database_id) == SQLITE_OK){
		switch(sqlite3_step(fetch_stmt)){
			case SQLITE_ROW:
				client_send(log, client, "+OK Here it comes\r\n");
				mail_data=(char*)sqlite3_column_text(fetch_stmt, 0);
				do{
					mail_bytestuff=strstr(mail_data, "\r\n.");
					if(mail_bytestuff){
						client_send(log, client, "%.*s\r\n..", mail_bytestuff-mail_data, mail_data);
						mail_data=mail_bytestuff+3;
					}
					else{
						client_send(log, client, "%s", mail_data);
					}
				}
				while(mail_bytestuff);
				break;
			default:
				logprintf(log, LOG_WARNING, "Failed to fetch mail: %s\n", sqlite3_errmsg(database->conn));
				client_send(log, client, "-ERR Failed to fetch mail\r\n");
				break;
		}
	}

	sqlite3_reset(fetch_stmt);
	sqlite3_clear_bindings(fetch_stmt);

	client_send(log, client, "\r\n.\r\n");
	return 0;
}

int pop_quit(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	
	if(client_data->state==STATE_TRANSACTION){
		//update the maildrop
		if(maildrop_update(log, database, &(client_data->maildrop))<0){
			client_send(log, client, "-ERR Failed to update the maildrop\r\n");
		}
		else{
			client_send(log, client, "+OK Maildrop updated\r\n");
		}
	}
	else{
		client_send(log, client, "+OK No change\r\n");
	}

	return client_close(log, client, database);
}

int pop_dele(LOGGER log, CONNECTION* client, DATABASE* database, unsigned mail){
	CLIENT* client_data=(CLIENT*)client->aux_data;

	if(mail>client_data->maildrop.count || mail<1){
		client_send(log, client, "-ERR No such mail\r\n");
		return -1;
	}

	if(client_data->maildrop.mails[mail-1].flag_delete){
		client_send(log, client, "-ERR Message already deleted\r\n");
		return -1;
	}

	client_data->maildrop.mails[mail-1].flag_delete=true;
	client_send(log, client, "+OK Marked for deletion\r\n");

	return 0;
}

int pop_rset(LOGGER log, CONNECTION* client, DATABASE* database){
	unsigned i;
	CLIENT* client_data=(CLIENT*)client->aux_data;

	for(i=0;i<client_data->maildrop.count;i++){
		client_data->maildrop.mails[i].flag_delete=false;
	}

	client_send(log, client, "+OK Deletion flags removed\r\n");
	return 0;
}

int pop_xyzzy(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data=(CLIENT*)client->aux_data;

	logprintf(log, LOG_INFO, "Client performs incantation\n");
	#ifndef CMAIL_NO_TLS
	logprintf(log, LOG_DEBUG, "Client TLS status: %s\n", (client_data->tls_mode==TLS_NONE)?"none":"ok");
	#endif
	client_send(log, client, "+OK Nothing happens\r\n");
	return 0;
}
