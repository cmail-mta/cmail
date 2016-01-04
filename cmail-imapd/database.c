int database_initialize(LOGGER log, DATABASE* database){
	char* QUERY_AUTHENTICATION_DATA = "SELECT user_authdata, user_alias FROM main.users WHERE user_name = ?;";
	char* QUERY_USER_DATABASE = "SELECT user_database FROM main.users WHERE user_name = ? AND user_database NOT NULL;";
	char* FETCH_MAIL_MASTER = "SELECT mail_data FROM main.mailbox WHERE mail_id=?;";

	//check the database schema version
	if(database_schema_version(log, database->conn) != CMAIL_CURRENT_SCHEMA_VERSION){
		logprintf(log, LOG_ERROR, "The database schema is at another version than required for this build\n");
		return -1;
	}

	database->query_authdata = database_prepare(log, database->conn, QUERY_AUTHENTICATION_DATA);
	database->query_userdatabase = database_prepare(log, database->conn, QUERY_USER_DATABASE);
	database->fetch_master = database_prepare(log, database->conn, FETCH_MAIL_MASTER);

	if(!database->query_authdata || !database->query_userdatabase){
		logprintf(log, LOG_ERROR, "Failed to prepare user data query\n");
		return -1;
	}

	if(!database->fetch_master){
		logprintf(log, LOG_ERROR, "Failed to prepare mail data query\n");
		return -1;
	}

	return 0;
}

int database_init_worker(LOGGER log, char* filename, WORKER_DATABASE* db){
	db->conn = database_open(log, filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX);

	if(!db->conn){
		logprintf(log, LOG_ERROR, "Failed to open database file %s\n", filename);
		return -1;
	}

	return 0;
}

void database_free_worker(WORKER_DATABASE* db){
	if(db->conn){
		sqlite3_finalize(db->mailbox_find);
		sqlite3_finalize(db->mailbox_info);
		sqlite3_finalize(db->mailbox_create);
		sqlite3_finalize(db->mailbox_delete);
		sqlite3_close(db->conn);
	}
}

void database_free(LOGGER log, DATABASE* database){
	//FIXME check for SQLITE_BUSY here

	if(database->conn){
		sqlite3_finalize(database->query_authdata);
		sqlite3_finalize(database->query_userdatabase);
		sqlite3_finalize(database->fetch_master);

		sqlite3_close(database->conn);
		database->conn = NULL;
	}
}
