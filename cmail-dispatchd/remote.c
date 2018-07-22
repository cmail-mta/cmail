int remote_reset(REMOTE* remote){
	REMOTE empty_remote = {
		.host = NULL,
		.mode = DELIVER_HANDOFF,
		.remote_auth = NULL,
		.forced_port = {},
		.remotespec = NULL
	};

	if(remote->host){
		free(remote->host);
	}

	if(remote->remotespec){
		free(remote->remotespec);
	}

	if(remote->remote_auth){
		free(remote->remote_auth);
	}

	*remote = empty_remote;

	return 0;
}

int remote_parse(REMOTE* remote, DELIVERY_MODE mode, char* remotespec){
	char* remote_separator = NULL;
	unsigned u;

	remote->remotespec = common_strdup(remotespec);
	remote->host = common_strdup(remotespec);
	remote->mode = mode;

	if(!remote->remotespec || !remote->host){
		logprintf(LOG_ERROR, "Failed to allocate memory for remote hostspec\n");
		return -1;
	}

	if(mode == DELIVER_HANDOFF){
		//parse additional handoff remote parameters
		//check for remote authentication
		if(index(remote->host, '@')){
			//copy remote authentication raw
			remote_separator = index(remote->host, '@');
			*remote_separator = 0;
			remote->remote_auth = strdup(remote->host);
			//FIXME error checking

			//move host part back to .host
			memmove(remote->host, remote_separator + 1, strlen(remote_separator + 1) + 1);

			//preprocess SASL response
			u = strlen(remote->remote_auth);
			remote->remote_auth = realloc(remote->remote_auth, (u + 2) * sizeof(char));
			memmove(remote->remote_auth + 1, remote->remote_auth, u + 1);
			remote_separator = index(remote->remote_auth, ':');
			if(!remote_separator){
				//TODO fail here
			}
			else{
				*remote_separator = 0;
			}
			*remote->remote_auth = 0;
			if(auth_base64encode((uint8_t**)&(remote->remote_auth), u + 2) < 0){
				//TODO fail here
			}
		}
		
		//check for port specification (and tls mode)
		if(index(remote->host, ':')){
			remote_separator = index(remote->host, ':');
			*remote_separator = 0;

			//parse remote port
			remote->forced_port.port = strtoul(remote_separator + 1, &remote_separator, 0);
			
			//check for tls modestring immediately following
			if(*remote_separator == '/'){
				#ifndef CMAIL_NO_TLS
				if(!strcasecmp(remote_separator, "/starttls")){
					remote->forced_port.tls_mode = TLS_NEGOTIATE;
				}
				else if(!strcasecmp(remote_separator, "/tls")){
					remote->forced_port.tls_mode = TLS_ONLY;
				}
				else{
					remote->forced_port.tls_mode = TLS_NONE;
					//FIXME might want to fail the remote upon invalid TLSMODE
				}
				#endif
			}
			else if(*remote_separator != 0){
				//invalid character, fail this remote
				logprintf(LOG_WARNING, "Trailing separator after portspec did not indicate TLS mode\n");
				return -1;
			}
		}					
	}

	return 0;
}

int remotes_fetch(DATABASE* database, MTA_SETTINGS settings, REMOTE** remotes, unsigned* remotes_allocated){
	int status;
	int remotes_active = 0;
	
	//bind retry timeout parameter
	if(sqlite3_bind_int(database->query_outbound_hosts, 1, settings.retry_interval) != SQLITE_OK){
		logprintf(LOG_ERROR, "Failed to bind mail retry timeout parameter\n");
		return -1;
	}

	//fetch all hosts to be delivered to
	do{
		status = sqlite3_step(database->query_outbound_hosts);
		switch(status){
			case SQLITE_ROW:
				if(remotes_active >= (*remotes_allocated)){
					//reallocate remote array
					*remotes = realloc(*remotes, ((*remotes_allocated) + CMAIL_REALLOC_CHUNK) * sizeof(REMOTE));
					if(!(*remotes)){
						logprintf(LOG_ERROR, "Failed to allocate memory for remote array\n");
						return -1;
					}

					//clear memory
					memset((*remotes) + (*remotes_allocated), 0, CMAIL_REALLOC_CHUNK * sizeof(REMOTE));
					(*remotes_allocated) += CMAIL_REALLOC_CHUNK;
				}

				//reset current entry
				remote_reset((*remotes) + remotes_active);

				if(sqlite3_column_text(database->query_outbound_hosts, 0)){
					if(remote_parse((*remotes) + remotes_active, DELIVER_HANDOFF, (char*)sqlite3_column_text(database->query_outbound_hosts, 0)) < 0){
						//FIXME check this failure mode
						logprintf(LOG_ERROR, "Failed to parse remote, skipping\n");
						remote_reset((*remotes) + remotes_active);
						continue;
					}
				}
				else if(sqlite3_column_text(database->query_outbound_hosts, 1)){
					if(remote_parse((*remotes) + remotes_active, DELIVER_DOMAIN, (char*)sqlite3_column_text(database->query_outbound_hosts, 1)) < 0){
						logprintf(LOG_ERROR, "Failed to parse remote, skipping\n");
						remote_reset((*remotes) + remotes_active);
						continue;
					}
				}
				else{
					logprintf(LOG_ERROR, "Invalid database remote\n");
				}

				remotes_active++;
				break;
			case SQLITE_DONE:
				//done reading remotes
				break;
			default:
				logprintf(LOG_WARNING, "Unhandled outbound host query result: %s\n", sqlite3_errmsg(database->conn));
				break;
		}
	}
	while(status == SQLITE_ROW);

	sqlite3_reset(database->query_outbound_hosts);
	sqlite3_clear_bindings(database->query_outbound_hosts);

	return remotes_active;	
}


