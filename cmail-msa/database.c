
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

int database_initialize(LOGGER log, DATABASE* database){
	char* QUERY_ATTACH_DB="ATTACH DATABASE ? AS ?;";
	char* QUERY_SELECT_DATABASES="SELECT user_name, user_inroute FROM users WHERE user_inrouter='store' AND user_inroute NOT NULL GROUP BY user_inroute;";
	int status=SQLITE_ROW;
	int rv=0;

	sqlite3_stmt* attach_db=database_prepare(log, database->conn, QUERY_ATTACH_DB);
	sqlite3_stmt* select_dbs=database_prepare(log, database->conn, QUERY_SELECT_DATABASES);
	
	if(!attach_db||!select_dbs){
		return -1;
	}

	while(status==SQLITE_ROW){
		status=sqlite3_step(select_dbs);
		switch(status){
			case SQLITE_ROW:
				logprintf(log, LOG_INFO, "Attaching database %s as %s\n", sqlite3_column_text(select_dbs, 1), sqlite3_column_text(select_dbs, 0));
				//TODO attach database
				//TODO compile insert statements
				break;
			case SQLITE_DONE:
				logprintf(log, LOG_INFO, "Database initialization done\n");
				break;
			default:
				logprintf(log, LOG_ERROR, "Database initialization failed: %s\n", sqlite3_errmsg(database->conn));
				rv=-1;
				break;
		}
	}
	
	sqlite3_finalize(attach_db);
	sqlite3_finalize(select_dbs);
	return rv;
}
