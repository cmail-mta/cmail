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

int database_schema_version(LOGGER log, sqlite3* conn){
	int status;
	char* QUERY_SCHEMA_VERSION="SELECT value FROM meta WHERE key='schema_version'";
	sqlite3_stmt* query_version=database_prepare(log, conn, QUERY_SCHEMA_VERSION);

	if(!query_version){
		logprintf(log, LOG_ERROR, "Failed to query the database schema version, your database file might not be a valid master database\n");
		return -1;
	}

	switch(sqlite3_step(query_version)){
		case SQLITE_ROW:
			status=strtoul((char*)sqlite3_column_text(query_version, 0), NULL, 10);
			logprintf(log, LOG_INFO, "Database schema version is %d\n", status);
			break;
		case SQLITE_DONE:
			logprintf(log, LOG_ERROR, "The database did not contain a schema_version key\n");
			status=-1;
			break;
		default:
			logprintf(log, LOG_ERROR, "Failed to get schema version: %s\n", sqlite3_errmsg(conn));
			status=-1;
	}

	sqlite3_finalize(query_version);
	return status;
}
