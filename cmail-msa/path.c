//TODO test this thoroughly
int path_parse(LOGGER log, char* pathspec, MAILPATH* path){
	//See http://cr.yp.to/smtp/address.html for hints on address parsing
	bool quotes=false, done_parsing=false;
	unsigned out_pos=0, in_pos=0;

	//skip leading spaces
	for(;isspace(pathspec[0]);pathspec++){
	}

	logprintf(log, LOG_DEBUG, "Parsing path %s\n", pathspec);

	for(in_pos=0;!done_parsing && out_pos<SMTP_MAX_PATH_LENGTH-1 && pathspec[in_pos];in_pos++){
		switch(pathspec[in_pos]){
			case '@':
				if(out_pos == 0){
					//route syntax. skip until next colon.
					for(;pathspec[in_pos] && pathspec[in_pos]!=':';in_pos++){
					}
					if(pathspec[in_pos] != ':'){
						//colon was somehow the last character. someone blew this.
						done_parsing = true;
					}
				}
				else{
					//copy to out buffer
					path->path[out_pos++] = pathspec[in_pos];
				}
				break;
			case '"':
				quotes = !quotes;
				break;
			case '\\':
				//escape character. add next char to out buffer
				//WARNING this can cause an issue when implemented incorrectly.
				//if the last character sent is a backslash escaping \0 the
				//loop potentially accesses memory out of bounds.
				//so, we check for that.
				//actually, in this implementation, there are at least
				//2 \0 bytes in that case, so this is a non-issue.
				//FIXME allow only printable/space characters here
				if(pathspec[in_pos+1]){
					in_pos++;
					if(isprint(pathspec[in_pos])){
						path->path[out_pos++] = pathspec[in_pos];
					}
				}
				else{
					done_parsing = true;
					break;
				}
				break;
			case '<':
				if(!quotes){
					//start mark. ignore it.
					break;
				}
				//fall through
			case '>':
				//FIXME allow only printable nonspace(?) characters here
				if(!quotes){
					done_parsing = true;
					break;
				}
				//fall through
			default:
				//copy to out buffer
				if(isprint(pathspec[in_pos])){
					path->path[out_pos++] = pathspec[in_pos];
				}
		}
	}

	path->path[out_pos] = 0;

	logprintf(log, LOG_DEBUG, "Result is %s\n", path->path);
	return 0;
}

// If originating_user is set, this checks if the user may use the path outbound
int path_resolve(LOGGER log, MAILPATH* path, DATABASE* database, char* originating_user, bool is_reverse){
	int status, rv = -1;
	char* router;

	if(path->resolved_user){
		return 0;
	}

	status = sqlite3_bind_text(database->query_addresses, 1, path->path, -1, SQLITE_STATIC);
	if(status == SQLITE_OK){
		do{
			status = sqlite3_step(database->query_addresses);

			if(originating_user || is_reverse){
				//test if matched user is authentiicated user
				if(originating_user && strcmp(originating_user, (char*)sqlite3_column_text(database->query_addresses, 0))){
					// Falling through to SQLITE_DONE here implies a non-local origin while routers for this adress are set
					// (just not for this user). The defined router will then fail the address, the any router will accept it
					continue;
				}
				router = (char*)sqlite3_column_text(database->query_addresses, 2);
			}
			else{
				router = (char*)sqlite3_column_text(database->query_addresses, 1);
			}

			//check for reject
			if(!strcmp(router, "reject")){
				rv = 1;
				break;
			}

			//path ok, resolve to user
			path->resolved_user = common_strdup((char*)sqlite3_column_text(database->query_addresses, 0));
			if(!path->resolved_user){
				logprintf(log, LOG_ERROR, "Failed to allocate path user data\n");
				break;
			}
			rv = 0;

		} while(status == SQLITE_ROW);

		switch(status){
			case SQLITE_ROW:
				//already handled during loop
				break;
			case SQLITE_DONE:
				logprintf(log, LOG_INFO, "No address match found\n");
				rv = 0;
				break;
			default:
				logprintf(log, LOG_ERROR, "Failed to query wildcard: %s\n", sqlite3_errmsg(database->conn));
				break;
		}
	}
	else{
		logprintf(log, LOG_ERROR, "Failed to bind search parameter: %s\n", sqlite3_errmsg(database->conn));
	}

	sqlite3_reset(database->query_addresses);
	sqlite3_clear_bindings(database->query_addresses);
	return rv;
}

void path_reset(MAILPATH* path){
	MAILPATH reset_path = {
		.in_transaction = false,
		.path = "",
		.resolved_user = NULL
	};

	if(path->resolved_user){
		free(path->resolved_user);
		path->resolved_user = NULL;
	}

	*path = reset_path;
}
