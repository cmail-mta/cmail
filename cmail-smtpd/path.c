//TODO test this thoroughly
int path_parse(char* pathspec, MAILPATH* path){
	//See http://cr.yp.to/smtp/address.html for hints on address parsing
	bool quotes = false, done_parsing = false, comment = false;
	unsigned out_pos = 0, in_pos = 0;

	//skip leading spaces
	for(; isspace(pathspec[0]); pathspec++){
	}

	logprintf(LOG_DEBUG, "Parsing path %s\n", pathspec);

	for(in_pos = 0; !done_parsing && out_pos < (SMTP_MAX_PATH_LENGTH - 1) && pathspec[in_pos]; in_pos++){
		if(!comment){
			switch(pathspec[in_pos]){
				case '@':
					if(out_pos == 0){
						//route syntax. skip until next colon.
						for(; pathspec[in_pos] && pathspec[in_pos] != ':'; in_pos++){
						}
						if(pathspec[in_pos] != ':'){
							//colon was somehow the last character. someone blew this.
							done_parsing = true;
						}
					}
					else{
						if(quotes){
							path->path[out_pos++] = pathspec[in_pos];
						}
						else if(path->delimiter_position == 0 && path->path[path->delimiter_position] != '@'){
							//copy to out buffer and update delimiter position
							path->delimiter_position = out_pos;
							path->path[out_pos++] = pathspec[in_pos];
						}
						else{
							logprintf(LOG_WARNING, "Multiple delimiters in path\n");
							return -1;
						}
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
					if(pathspec[in_pos + 1]){
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
				case '(':
					//comment delimiter
					comment = true;
					break;
				case ')':
					//comment closed without active comment context
					logprintf(LOG_WARNING, "Path contained illegal parenthesis\n");
					return -1;
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
		else if(pathspec[in_pos] == ')'){
			comment = false;
		}
	}

	path->path[out_pos] = 0;

	if(comment){
		logprintf(LOG_WARNING, "Path contains unterminated comment\n");
		return -1;
	}

	if(!path->delimiter_position){
		path->delimiter_position = strlen(path->path);
	}

	logprintf(LOG_DEBUG, "Result is %s, delimiter is at %d\n", path->path, path->delimiter_position);
	return 0;
}

// If originating_user is set, this checks if the user may use the path outbound
int path_resolve(MAILPATH* path, DATABASE* database, char* originating_user, bool is_reverse){
	int status, rv = -1;

	//this early exit should never have to be taken
	if(path->route.router){
		logprintf(LOG_WARNING, "Taking early exit for path %s, please notify the developers\n", path->path);
		return 0;
	}

	status = sqlite3_bind_text(database->query_address, 1, path->path, -1, SQLITE_STATIC);
	if(status == SQLITE_OK){
		do{
			status = sqlite3_step(database->query_address);
			if(status == SQLITE_ROW){
				if(originating_user || is_reverse){
					//Test whether the originating_user may user the supplied path outbound.
					//This implies traversing all entries and testing the following conditions
					//	1. Router must be 'store'
					//	2. Route must be the originating user
					//	Else, try the next entry
					if(originating_user && 
						(strcmp((char*)sqlite3_column_text(database->query_address, 0), "store") 
							|| strcmp(originating_user, (char*)sqlite3_column_text(database->query_address, 1)))){
						// Falling through to SQLITE_DONE here implies a non-local origin while routers for this adress are set
						// (just not for this user). The defined router will then fail the address, the any router will accept it
						continue;
					}

					//Continuing here if no originating_user given or the current routing information
					//applies to the originating_user
				}

				//heap-copy the routing information
				path->route.router = common_strdup((char*)sqlite3_column_text(database->query_address, 0));
				if(sqlite3_column_text(database->query_address, 1)){
					path->route.argument = common_strdup((char*)sqlite3_column_text(database->query_address, 1));
				}

				if(!path->route.router){
					logprintf(LOG_ERROR, "Failed to allocate storage for routing data\n");
					//fail temporarily
					rv = -1;
					break;
				}

				//check for reject
				if(!strcmp(path->route.router, "reject")){
					rv = 1;
					break;
				}

				//all is well
				rv = 0;
				break;
			}
		} while(status == SQLITE_ROW);

		switch(status){
			case SQLITE_ROW:
				//already handled during loop
				break;
			case SQLITE_DONE:
				logprintf(LOG_INFO, "No address match found\n");
				//continue with this path marked as non-local
				rv = 0;
				break;
			default:
				logprintf(LOG_ERROR, "Failed to query wildcard: %s\n", sqlite3_errmsg(database->conn));
				rv = -1;
				break;
		}
	}
	else{
		logprintf(LOG_ERROR, "Failed to bind search parameter: %s\n", sqlite3_errmsg(database->conn));
		rv = -1;
	}

	sqlite3_reset(database->query_address);
	sqlite3_clear_bindings(database->query_address);

	// Calling contract
	// 	0 -> Accept path
	// 	1 -> Reject path (500), if possible use router argument
	// 	* -> Reject path (400)
	return rv;
}

void path_reset(MAILPATH* path){
	MAILPATH reset_path = {
		.delimiter_position = 0,
		.in_transaction = false,
		.path = "",
		.route = {
			.router = NULL,
			.argument = NULL
		}
	};

	route_free(&(path->route));

	*path = reset_path;
}
