
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

int database_attach(LOGGER log, sqlite3_stmt* attach, char* database, char* name){
	int status, rv=0;
	
	status=sqlite3_bind_text(attach, 1, database, -1, SQLITE_STATIC);
	if(status!=SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind database parameter\n");
		sqlite3_reset(attach);
		sqlite3_clear_bindings(attach);
		return -1;
	}

	status=sqlite3_bind_text(attach, 2, name, -1, SQLITE_STATIC);
	if(status!=SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind database name parameter\n");
		sqlite3_reset(attach);
		sqlite3_clear_bindings(attach);
		return -1;
	}

	status=sqlite3_step(attach);

	switch(status){
		case SQLITE_DONE:
			logprintf(log, LOG_INFO, "Attached database %s as %s\n", database, name);
			//FIXME check result
			break;
		case SQLITE_ERROR:
			rv=-1;
			break;
		default:
			logprintf(log, LOG_WARNING, "Uncaught attach response code %d\n", status);
			rv=1;
			break;
	}

	sqlite3_reset(attach);
	sqlite3_clear_bindings(attach);
	return rv;
}

int database_initialize(LOGGER log, DATABASE* database){
	char* QUERY_ATTACH_DB="ATTACH DATABASE ? AS ?;";
	char* QUERY_SELECT_DATABASES="SELECT MIN(user_name), user_inroute FROM users WHERE user_inrouter='store' AND user_inroute NOT NULL GROUP BY user_inroute;";
	char* QUERY_ADDRESS_USER="SELECT address_user FROM addresses WHERE ? LIKE address_expression ORDER BY address_order ASC;";
	char* QUERY_USER_ROUTER_INBOUND="SELECT user_inrouter, user_inroute FROM users WHERE user_name = ?;";
	char* QUERY_USER_ROUTER_OUTBOUND="SELECT user_outrouter, user_outroute FROM users WHERE user_name = ?;";
	char* INSERT_MAIL_MASTER="INSERT INTO mailbox (mail_user, mail_envelopeto, mail_envelopefrom, mail_submitter, mail_data) VALUES (?, ?, ?, ?, ?);";
	int status=SQLITE_ROW;
	int rv=0;

	sqlite3_stmt* attach_db=database_prepare(log, database->conn, QUERY_ATTACH_DB);
	sqlite3_stmt* select_dbs=database_prepare(log, database->conn, QUERY_SELECT_DATABASES);
	
	database->query_addresses=database_prepare(log, database->conn, QUERY_ADDRESS_USER);
	database->query_inrouter=database_prepare(log, database->conn, QUERY_USER_ROUTER_INBOUND);
	database->query_outrouter=database_prepare(log, database->conn, QUERY_USER_ROUTER_OUTBOUND);
	database->mail_storage.store_master=database_prepare(log, database->conn, INSERT_MAIL_MASTER);

	if(!attach_db||!select_dbs){
		logprintf(log, LOG_ERROR, "Failed to prepare auxiliary attach statements\n");
		return -1;
	}

	if(!database->query_addresses){
		logprintf(log, LOG_ERROR, "Failed to prepare address query statement\n");
		return -1;
	}

	if(!database->query_inrouter || !database->query_outrouter){
		logprintf(log, LOG_ERROR, "Failed to prepare router query statement\n");
		return -1;
	}

	if(!database->mail_storage.store_master){
		logprintf(log, LOG_ERROR, "Failed to prepare mail storage statement\n");
		return -1;
	}

	//attach user databases if needed
	do{
		status=sqlite3_step(select_dbs);
		switch(status){
			case SQLITE_ROW:
				//attach the user database
				switch(database_attach(log, attach_db, (char*)sqlite3_column_text(select_dbs, 1), (char*)sqlite3_column_text(select_dbs, 0))){
					case -1:
						logprintf(log, LOG_ERROR, "Failed to attach database: %s\n", sqlite3_errmsg(database->conn));
						status=SQLITE_ERROR;
						rv=-1;
						break;
					case 1:
						logprintf(log, LOG_ERROR, "Additional Information: %s\n", sqlite3_errmsg(database->conn));
					default:
						//TODO compile insert statement

						break;
				}
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
	while(status==SQLITE_ROW);
	
	sqlite3_finalize(attach_db);
	sqlite3_finalize(select_dbs);
	return rv;
}

void database_free(DATABASE* database){
	//FIXME check for SQLITE_BUSY here
	if(database->conn){
		sqlite3_finalize(database->query_addresses);
		sqlite3_finalize(database->query_inrouter);
		sqlite3_finalize(database->query_outrouter);
		sqlite3_finalize(database->mail_storage.store_master);
		
		sqlite3_close(database->conn);
		database->conn=NULL;
	}
	else{
		return;
	}
}
