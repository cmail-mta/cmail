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

int database_query_mailbox(LOGGER log, WORKER_DATABASE* db, char* user, char* mailbox, int parent){
	int rv = -1;
	int result;

	//all INBOXes are 0
	if(parent < 0 && !strcasecmp(mailbox, "inbox")){
		return 0;
	}

	//cannot bind parent directly since NULL is a valid parent (and that cannot be expressed in an = expression)
	if(sqlite3_bind_text(db->mailbox_find, 1, mailbox, -1, SQLITE_STATIC) == SQLITE_OK
			&& sqlite3_bind_text(db->mailbox_find, 2, user, -1, SQLITE_STATIC) == SQLITE_OK){

		do{
			result = sqlite3_step(db->mailbox_find);
			switch(result){
				case SQLITE_DONE:
					logprintf(log, LOG_DEBUG, "Mailbox %s with parent %d does not exist for user %s\n", mailbox, parent, user);
					break;
				case SQLITE_ROW:
					if((parent < 0 && sqlite3_column_type(db->mailbox_find, 1) == SQLITE_NULL)
							|| (parent >= 0 && sqlite3_column_int(db->mailbox_find, 1) == parent)){
						rv = sqlite3_column_int(db->mailbox_find, 0);
						logprintf(log, LOG_DEBUG, "Mailbox %s with parent %d for user %s has ID %d\n", mailbox, parent, user, rv);
						//break out of the loop
						result = SQLITE_DONE;
					}
					break;
				default:
					logprintf(log, LOG_ERROR, "Failed to query for mailbox: %s\n", sqlite3_errmsg(db->conn));
					rv = -2;
					break;
			}
		}
		while(result == SQLITE_ROW);
	}
	else{
		logprintf(log, LOG_ERROR, "Failed to bind parameter to mailbox search query\n");
	}

	sqlite3_clear_bindings(db->mailbox_find);
	sqlite3_reset(db->mailbox_find);

	return rv;
}

int database_create_mailbox(LOGGER log, WORKER_DATABASE* db, char* user, char* mailbox, int parent){
	int rv = -1;

	//creating inbox is always an error
	if(parent < 0 && !strcasecmp(mailbox, "inbox")){
		return -1;
	}

	if(sqlite3_bind_text(db->mailbox_create, 1, mailbox, -1, SQLITE_STATIC) == SQLITE_OK
			&& ((parent < 0) ? sqlite3_bind_null(db->mailbox_find, 2):sqlite3_bind_int(db->mailbox_create, 2, parent)) == SQLITE_OK
			&& sqlite3_bind_text(db->mailbox_create, 3, user, -1, SQLITE_STATIC) == SQLITE_OK){
		switch(sqlite3_step(db->mailbox_create)){
			case SQLITE_DONE:
			case SQLITE_OK:
				rv = sqlite3_last_insert_rowid(db->conn);
				logprintf(log, LOG_DEBUG, "Mailbox %s with parent %d created for user %s, id %d\n", mailbox, parent, user, rv);
				break;
			default:
				logprintf(log, LOG_ERROR, "Failed to create mailbox: %s\n", sqlite3_errmsg(db->conn));
				rv = -2;
				break;
		}
	}
	else{
		logprintf(log, LOG_ERROR, "Failed to bind parameter to mailbox create query\n");
	}

	sqlite3_clear_bindings(db->mailbox_create);
	sqlite3_reset(db->mailbox_create);

	return rv;
}

int database_init_worker(LOGGER log, char* filename, WORKER_DATABASE* db, bool is_master){
	char* FIND_MAILBOX = "SELECT mailbox_id, mailbox_parent FROM mailbox_names WHERE mailbox_name = ? AND mailbox_user = ?;";
	char* MAILBOX_INFO = "SELECT COUNT(*) FROM mailbox_mapping WHERE mailbox_id = ?;";
	char* CREATE_MAILBOX = "INSERT INTO mailbox_names (mailbox_name, mailbox_parent, mailbox_user) VALUES (?, ?, ?);";
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
	WORKER_DATABASE empty = {
		.conn = NULL,
		.mailbox_find = NULL,
		.mailbox_info = NULL,
		.mailbox_create = NULL,
		.mailbox_delete = NULL,
		.fetch = NULL,
		.query_userdatabase = NULL
	};

	if(db->conn){
		sqlite3_finalize(db->mailbox_find);
		sqlite3_finalize(db->mailbox_info);
		sqlite3_finalize(db->mailbox_create);
		sqlite3_finalize(db->mailbox_delete);
		sqlite3_finalize(db->fetch);
		sqlite3_finalize(db->query_userdatabase);

		sqlite3_close(db->conn);
	}

	*db = empty;
}

void database_free(LOGGER log, DATABASE* database){
	//FIXME check for SQLITE_BUSY here

	if(database->conn){
		sqlite3_finalize(database->query_authdata);

		sqlite3_close(database->conn);
		database->conn = NULL;
	}
}
