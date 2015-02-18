
sqlite3_stmt* database_prepare(LOGGER log, sqlite3* master, char* query){
	int status;
	sqlite3_stmt* target=NULL;

	status=sqlite3_prepare_v2(master, query, strlen(query), &target, NULL);

	switch(status){
		case SQLITE_OK:
			logprintf(log, LOG_DEBUG, "Statement (%s) compiled ok\n", query);
			return target;
		default:
			logprintf(log, LOG_ERROR, "Failed to prepare statement (%s): %s\n", query, sqlite3_errmsg(master));
	}

	return NULL;
}

int database_initialize(LOGGER log, sqlite3* master){
	char* QUERY_ATTACH_DB="ATTACH DATABASE ? AS ?;";
	char* QUERY_SELECT_DATABASES="SELECT user_name, user_route FROM users WHERE user_router='store' AND user_route NOT NULL GROUP BY user_route;";
	int status=SQLITE_ROW;
	int rv=0;

	sqlite3_stmt* attach_db=database_prepare(log, master, QUERY_ATTACH_DB);
	sqlite3_stmt* select_dbs=database_prepare(log, master, QUERY_SELECT_DATABASES);
	
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
				logprintf(log, LOG_ERROR, "Database initialization failed: %s\n", sqlite3_errmsg(master));
				rv=-1;
				break;
		}
	}
	
	sqlite3_finalize(attach_db);
	sqlite3_finalize(select_dbs);
	return rv;
}
