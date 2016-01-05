int database_initialize(LOGGER log, DATABASE* database){
	char* QUERY_AUTHENTICATION_DATA = "SELECT user_authdata, user_alias FROM main.users WHERE user_name = ?;";

	//check the database schema version
	if(database_schema_version(log, database->conn) != CMAIL_CURRENT_SCHEMA_VERSION){
		logprintf(log, LOG_ERROR, "The database schema is at another version than required for this build\n");
		return -1;
	}

	database->query_authdata = database_prepare(log, database->conn, QUERY_AUTHENTICATION_DATA);

	if(!database->query_authdata){
		logprintf(log, LOG_ERROR, "Failed to prepare user data query\n");
		return -1;
	}

	return 0;
}

int database_init_worker(LOGGER log, char* filename, WORKER_DATABASE* db, bool is_master){
	char* FIND_MAILBOX = "SELECT mailbox_id FROM mailbox_names WHERE mailbox_name = ? AND mailbox_parent = ?;";
	char* MAILBOX_INFO = "SELECT COUNT(*) FROM mailbox_mapping WHERE mailbox_id = ?;";
	char* CREATE_MAILBOX = "INSERT INTO mailbox_names (mailbox_name, mailbox_parent) VALUES (?, ?);";
	char* DELETE_MAILBOX = "DELETE FROM mailbox_names WHERE mailbox_name = ? AND mailbox_parent = ?;";
	char* QUERY_USER_DATABASE = "SELECT user_database FROM main.users WHERE user_name = ? AND user_database NOT NULL;";
	char* FETCH_MAIL = "SELECT mail_data FROM main.mailbox WHERE mail_id = ?;";

	db->conn = database_open(log, filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX);

	if(!db->conn){
		logprintf(log, LOG_ERROR, "Failed to open database file %s\n", filename);
		return -1;
	}

	db->mailbox_find = database_prepare(log, db->conn, FIND_MAILBOX);
	db->mailbox_info = database_prepare(log, db->conn, MAILBOX_INFO);
	db->mailbox_create = database_prepare(log, db->conn, CREATE_MAILBOX);
	db->mailbox_delete = database_prepare(log, db->conn, DELETE_MAILBOX);
	db->fetch = database_prepare(log, db->conn, FETCH_MAIL);

	if(is_master){
		db->query_userdatabase = database_prepare(log, db->conn, QUERY_USER_DATABASE);

		if(!db->query_userdatabase){
			logprintf(log, LOG_ERROR, "Failed to prepare user database location query\n");
			return -1;
		}
	}

	if(!db->mailbox_find || !db->mailbox_info){
		logprintf(log, LOG_ERROR, "Failed to prepare mailbox info queries\n");
		return -1;
	}

	if(!db->mailbox_create || !db->mailbox_delete){
		logprintf(log, LOG_ERROR, "Failed to prepare mailbox management queries\n");
		return -1;
	}

	if(!db->fetch){
		logprintf(log, LOG_ERROR, "Failed to prepare mail data query\n");
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
		sqlite3_finalize(db->fetch);
		sqlite3_finalize(db->query_userdatabase);

		sqlite3_close(db->conn);
	}
}

void database_free(LOGGER log, DATABASE* database){
	//FIXME check for SQLITE_BUSY here

	if(database->conn){
		sqlite3_finalize(database->query_authdata);

		sqlite3_close(database->conn);
		database->conn = NULL;
	}
}
