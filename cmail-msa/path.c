//TODO test this
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
				if(out_pos==0){
					//route syntax. skip until next colon.
					for(;pathspec[in_pos] && pathspec[in_pos]!=':';in_pos++){
					}
					if(pathspec[in_pos]!=':'){
						//colon was somehow the last character. someone blew this.
						done_parsing=true;
					}
				}
				else{
					//copy to out buffer
					path->path[out_pos++]=pathspec[in_pos];
				}
				break;
			case '"':
				quotes=!quotes;
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
					path->path[out_pos++]=pathspec[++in_pos];
				}
				else{
					done_parsing=true;
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
				//FIXME allow only printable/space characters here
				if(!quotes){
					done_parsing=true;
					break;
				}
				//fall through
			default:
				//copy to out buffer
				path->path[out_pos++]=pathspec[in_pos];
		}
	}

	path->path[out_pos]=0;

	logprintf(log, LOG_DEBUG, "Result is %s\n", path->path);
	return 0;
}

int path_resolve(LOGGER log, MAILPATH* path, sqlite3* master){
	//TODO
	return -1;
}

void path_reset(MAILPATH* path){
	MAILPATH reset_path = {
		.in_transaction = false,
		.path = "",
		.resolved_user = NULL
	};

	if(path->resolved_user){
		free(path->resolved_user);
		path->resolved_user=NULL;
	}

	*path=reset_path;
}
