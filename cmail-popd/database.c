int database_initialize(LOGGER log, DATABASE* database){
	char* QUERY_AUTHENTICATION_DATA="SELECT user_authdata FROM main.users WHERE user_name = ?;";

	database->query_authdata=database_prepare(log, database->conn, QUERY_AUTHENTICATION_DATA);
	
	if(!database->query_authdata){
		logprintf(log, LOG_ERROR, "Failed to prepare authentication data query\n");
		return -1;
	}

	return 0;
}

void database_free(LOGGER log, DATABASE* database){
	//FIXME check for SQLITE_BUSY here
	
	if(database->conn){
		sqlite3_finalize(database->query_authdata);
		sqlite3_close(database->conn);
		database->conn=NULL;
	}
}
