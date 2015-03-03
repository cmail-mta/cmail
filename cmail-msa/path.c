int path_parse(LOGGER log, char* pathspec, MAILPATH* path){
	//TODO
	logprintf(log, LOG_DEBUG, "Parsing path %s\n", pathspec);
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
	}

	*path=reset_path;
}

