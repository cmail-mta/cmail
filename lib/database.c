sqlite3_stmt* database_prepare(LOGGER log, sqlite3* conn, char* query){
	int status;
	sqlite3_stmt* target=NULL;

	status=sqlite3_prepare_v2(conn, query, strlen(query), &target, NULL);

	switch(status){
		case SQLITE_OK:
			logprintf(log, LOG_DEBUG, "Statement (%s) compiled ok\n", query);
			return target;
		default:
			logprintf(log, LOG_ERROR, "Failed to prepare statement (%s): %s\n", query, sqlite3_errmsg(conn));
	}

	return NULL;
}