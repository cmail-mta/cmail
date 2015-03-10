
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
	
	if(sqlite3_bind_text(attach, 1, database, -1, SQLITE_STATIC)==SQLITE_OK
			&& sqlite3_bind_text(attach, 2, name, -1, SQLITE_STATIC)==SQLITE_OK){
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
	}
	else{
		logprintf(log, LOG_ERROR, "Failed to bind attach statement parameter\n");
		rv=-1;
	}

	sqlite3_reset(attach);
	sqlite3_clear_bindings(attach);
	return rv;
}

int database_detach(LOGGER log, sqlite3_stmt* detach, char* database){
	int status, rv=0;
	
	if(sqlite3_bind_text(detach, 1, database, -1, SQLITE_STATIC)==SQLITE_OK){
		status=sqlite3_step(detach);

		switch(status){
			case SQLITE_DONE:
				logprintf(log, LOG_INFO, "Detached database %s\n", database);
				//FIXME check result
				break;
			case SQLITE_ERROR:
				rv=-1;
				break;
			default:
				logprintf(log, LOG_WARNING, "Uncaught detach response code %d\n", status);
				rv=1;
				break;
		}
	}
	else{
		logprintf(log, LOG_ERROR, "Failed to bind detach statement parameter\n");
		rv=-1;
	}
	
	sqlite3_reset(detach);
	sqlite3_clear_bindings(detach);
	return rv;
}

int database_refresh(LOGGER log, DATABASE* database){
	int status, rv=0;
	
	char* QUERY_ATTACH_DB="ATTACH DATABASE ? AS ?;";
	char* QUERY_DETACH_DB="DETACH DATABASE ?;";
	char* QUERY_USER_DATABASES="SELECT MIN(user_name), user_inroute FROM main.users WHERE user_inrouter='store' AND user_inroute NOT NULL GROUP BY user_inroute;";
	
	sqlite3_stmt* attach_db=database_prepare(log, database->conn, QUERY_ATTACH_DB);
	sqlite3_stmt* detach_db=database_prepare(log, database->conn, QUERY_DETACH_DB);
	sqlite3_stmt* select_dbs=database_prepare(log, database->conn, QUERY_USER_DATABASES);
	
	if(!attach_db||!detach_db||!select_dbs){
		logprintf(log, LOG_ERROR, "Failed to prepare user storage management statements\n");
		return -1;
	}
	
	do{
		//fetch user database
		status=sqlite3_step(select_dbs);
		switch(status){
			case SQLITE_ROW:
				//TODO if not attached, attach
				//TODO mark database active
				//switch(database_attach(log, attach_db, (char*)sqlite3_column_text(select_dbs, 1), (char*)sqlite3_column_text(select_dbs, 0))){
				//	case -1:
				//		logprintf(log, LOG_ERROR, "Failed to attach database: %s\n", sqlite3_errmsg(database->conn));
				//		status=SQLITE_ERROR;
				//		rv=-1;
				//		break;
				//	case 1:
				//		logprintf(log, LOG_ERROR, "Additional Information: %s\n", sqlite3_errmsg(database->conn));
				//	default:
				//		//TODO compile insert statement
				//		break;
				//}
				break;
			case SQLITE_DONE:
				break;
			default:
				logprintf(log, LOG_ERROR, "User storage database initialization failed: %s\n", sqlite3_errmsg(database->conn));
				rv=-1;
				break;
		}
	}
	while(status==SQLITE_ROW);

	//TODO detach inactive databases

	sqlite3_finalize(attach_db);
	sqlite3_finalize(detach_db);
	sqlite3_finalize(select_dbs);
	return rv;
}

int database_initialize(LOGGER log, DATABASE* database){
	char* QUERY_ADDRESS_USER="SELECT address_user FROM main.addresses WHERE ? LIKE address_expression ORDER BY address_order ASC;";
	char* QUERY_USER_ROUTER_INBOUND="SELECT user_inrouter, user_inroute FROM main.users WHERE user_name = ?;";
	char* QUERY_USER_ROUTER_OUTBOUND="SELECT user_outrouter, user_outroute FROM main.users WHERE user_name = ?;";
	char* INSERT_MASTER_MAILBOX="INSERT INTO main.mailbox (mail_user, mail_envelopeto, mail_envelopefrom, mail_submitter, mail_data) VALUES (?, ?, ?, ?, ?);";
	char* INSERT_MASTER_OUTBOX="INSERT INTO main.outbox (mail_remote, mail_envelopefrom, mail_envelopeto, mail_data) VALUES (?, ?, ?, ?);";
	
	database->query_addresses=database_prepare(log, database->conn, QUERY_ADDRESS_USER);
	database->query_inrouter=database_prepare(log, database->conn, QUERY_USER_ROUTER_INBOUND);
	database->query_outrouter=database_prepare(log, database->conn, QUERY_USER_ROUTER_OUTBOUND);
	database->mail_storage.mailbox_master=database_prepare(log, database->conn, INSERT_MASTER_MAILBOX);
	database->mail_storage.outbox_master=database_prepare(log, database->conn, INSERT_MASTER_OUTBOX);

	if(!database->query_addresses){
		logprintf(log, LOG_ERROR, "Failed to prepare address query statement\n");
		return -1;
	}

	if(!database->query_inrouter || !database->query_outrouter){
		logprintf(log, LOG_ERROR, "Failed to prepare router query statement\n");
		return -1;
	}

	if(!database->mail_storage.mailbox_master || !database->mail_storage.outbox_master){
		logprintf(log, LOG_ERROR, "Failed to prepare mail storage statement\n");
		return -1;
	}

	return database_refresh(log, database);
}

void database_free(DATABASE* database){
	//FIXME check for SQLITE_BUSY here
	if(database->conn){
		sqlite3_finalize(database->query_addresses);
		sqlite3_finalize(database->query_inrouter);
		sqlite3_finalize(database->query_outrouter);
		sqlite3_finalize(database->mail_storage.mailbox_master);
		sqlite3_finalize(database->mail_storage.outbox_master);
		
		sqlite3_close(database->conn);
		database->conn=NULL;
	}
	else{
		return;
	}
}
