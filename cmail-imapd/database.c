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

int database_aggregate_query(LOGGER log, int* result, sqlite3_stmt* stmt, unsigned params, ...){
	va_list args;
	unsigned u;

	if(!stmt || !result){
		return -1;
	}

	va_start(args, params);

	for(u = 0; u < params; u++){
		switch(va_arg(args, int)){
			case ARG_INTEGER:
				if(sqlite3_bind_int(stmt, u + 1, va_arg(args, int)) != SQLITE_OK){
					logprintf(log, LOG_ERROR, "Failed to bind integer argument to aggregate query in position %d\n", u + 1);
					va_end(args);
					return -1;
				}
				break;
			case ARG_STRING:
				if(sqlite3_bind_text(stmt, u + 1, va_arg(args, char*), -1, SQLITE_STATIC) != SQLITE_OK){
					logprintf(log, LOG_ERROR, "Failed to bind string argument to aggregate query in position %d\n", u + 1);
					va_end(args);
					return -1;
				}
				break;
			default:
				logprintf(log, LOG_ERROR, "Invalid argument type to database_aggregate_query\n");
				va_end(args);
				return -1;
		}
	}

	va_end(args);

	if(sqlite3_step(stmt) != SQLITE_ROW){
		logprintf(log, LOG_ERROR, "Failed to execute aggregate query\n");
		return -1;
	}

	*result = sqlite3_column_int(stmt, 0);

	sqlite3_clear_bindings(stmt);
	sqlite3_reset(stmt);

	logprintf(log, LOG_DEBUG, "Aggregate result is %d\n", *result);
	return 0;
}

int database_query_mailbox(LOGGER log, WORKER_DATABASE* db, char* user, char* mailbox){
	int rv = -1;
	int result;

	//all INBOXes are 0
	if(!strcasecmp(mailbox, "inbox")){
		return 0;
	}

	if(sqlite3_bind_text(db->mailbox_find, 1, mailbox, -1, SQLITE_STATIC) == SQLITE_OK
			&& sqlite3_bind_text(db->mailbox_find, 2, user, -1, SQLITE_STATIC) == SQLITE_OK){

		do{
			result = sqlite3_step(db->mailbox_find);
			switch(result){
				case SQLITE_DONE:
					logprintf(log, LOG_DEBUG, "Mailbox %s does not exist for user %s\n", mailbox, user);
					break;
				case SQLITE_ROW:
					rv = sqlite3_column_int(db->mailbox_find, 0);
					logprintf(log, LOG_DEBUG, "Mailbox %s for user %s has ID %d\n", mailbox, user, rv);
					//break out of the loop
					result = SQLITE_DONE;
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

size_t database_mailbox_inferiors(WORKER_DATABASE* db, int mailbox, char* user, int** inferior_id){
	int status = SQLITE_DONE;
	size_t inferiors = 0;

	//sanity check
	if(!user){
		//TODO log output
		return 0;
	}

	if(sqlite3_bind_int(db->mailbox_find_inferiors, 1, mailbox) == SQLITE_OK
			&& sqlite3_bind_text(db->mailbox_find_inferiors, 2, user, -1, SQLITE_STATIC) == SQLITE_OK){
		do{
			status = sqlite3_step(db->mailbox_find_inferiors);
			switch(status){
				case SQLITE_ROW:
					//FIXME this might be problematic
					if(inferior_id){
						*inferior_id = realloc(*inferior_id, (inferiors + 1) * sizeof(int));
						if(!(*inferior_id)){
							//TODO log output
						}
						(*inferior_id)[inferiors] = sqlite3_column_int(db->mailbox_find_inferiors, 0);
					}
					inferiors++;
					break;
				case SQLITE_DONE:
				case SQLITE_OK:
					break;
				default:
					//TODO log output
					break;
			}
		} while(status != SQLITE_DONE);
	}
	else{
		//TODO log output
	}

	sqlite3_clear_bindings(db->mailbox_find_inferiors);
	sqlite3_reset(db->mailbox_find_inferiors);
	return inferiors;
}

int database_delete_mailbox_mail(WORKER_DATABASE* db, int mailbox, char* user){
	int rv = 0;

	//sanity check
	if(mailbox <= 0 || !user){
		//TODO log output
		return -1;
	}

	if(sqlite3_bind_int(db->mailbox_delete_contents, 1, mailbox) == SQLITE_OK
			&& sqlite3_bind_text(db->mailbox_delete_contents, 2, user, -1, SQLITE_STATIC) == SQLITE_OK){
		switch(sqlite3_step(db->mailbox_delete_contents)){
			case SQLITE_DONE:
			case SQLITE_OK:
				//TODO log output
				break;
			default:
				//TODO log output
				rv = -2;
				break;
		}

	}
	else{
		//TODO log output
		rv = -1;
	}

	sqlite3_clear_bindings(db->mailbox_delete_contents);
	sqlite3_reset(db->mailbox_delete_contents);

	return rv;
}

int database_delete_mailbox(WORKER_DATABASE* db, int mailbox, char* user){
	int rv = 0;

	//sanity check
	if(mailbox <= 0 || !user){
		//TODO log output
		return -1;
	}

	if(sqlite3_bind_int(db->mailbox_delete, 1, mailbox) == SQLITE_OK
			&& sqlite3_bind_text(db->mailbox_delete, 2, user, -1, SQLITE_STATIC) == SQLITE_OK){
		switch(sqlite3_step(db->mailbox_delete)){
			case SQLITE_DONE:
			case SQLITE_OK:
				//TODO log output
				break;
			default:
				//TODO log output
				rv = -2;
				break;
		}

	}
	else{
		//TODO log output
		rv = -1;
	}

	sqlite3_clear_bindings(db->mailbox_delete);
	sqlite3_reset(db->mailbox_delete);

	return rv;
}

int database_init_worker(LOGGER log, char* filename, WORKER_DATABASE* db, bool is_master){
	char* FIND_MAILBOX = "SELECT mailbox_id, mailbox_parent FROM mailbox_names WHERE mailbox_name = ? AND mailbox_user = ?;";
	char* FIND_MAILBOX_INFERIORS = "SELECT mailbox_id FROM mailbox_names WHERE mailbox_name LIKE (SELECT mailbox_name FROM mailbox_names WHERE mailbox_id = ?) || '/\%' AND mailbox_user = ?;";
	char* CREATE_MAILBOX = "INSERT INTO mailbox_names (mailbox_name, mailbox_parent, mailbox_user) VALUES (?, ?, ?);";
	char* DELETE_MAILBOX = "DELETE FROM mailbox_names WHERE mailbox_id = ? AND mailbox_user = ?;";
	char* DELETE_MAILBOX_CONTENTS = "DELETE FROM mailbox WHERE mail_id IN (SELECT mail_id FROM mailbox_mapping WHERE mailbox_id = ?) AND mail_user = ?;";
	char* QUERY_USER_DATABASE = "SELECT user_database FROM main.users WHERE user_name = ? AND user_database NOT NULL;";
	char* FETCH_MAIL = "SELECT mail_data FROM main.mailbox WHERE mail_id = ?;";

	char* SELECTION_EXISTS = "SELECT COUNT(*) FROM mailbox_mapping WHERE mailbox_id = ?;";
	char* INBOX_EXISTS = "SELECT COUNT(*) FROM mailbox WHERE mail_user = ? AND mail_id NOT IN (SELECT (mail_id) FROM mailbox_mapping);";
	char* SELECTION_RECENT = "SELECT COUNT(*) FROM mailbox_mapping WHERE mailbox_id = ? AND mail_id NOT IN (SELECT (mail_id) FROM flags);";
	char* INBOX_RECENT = "SELECT COUNT(*) FROM mailbox WHERE mail_user = ? AND mail_id NOT IN (SELECT (mail_id) FROM flags) AND mail_id NOT IN (SELECT (mail_id) FROM mailbox_mapping);";

	db->conn = database_open(log, filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX);

	if(!db->conn){
		logprintf(log, LOG_ERROR, "Failed to open database file %s\n", filename);
		return -1;
	}

	db->mailbox_find = database_prepare(log, db->conn, FIND_MAILBOX);
	db->mailbox_find_inferiors = database_prepare(log, db->conn, FIND_MAILBOX_INFERIORS);
	db->mailbox_create = database_prepare(log, db->conn, CREATE_MAILBOX);
	db->mailbox_delete = database_prepare(log, db->conn, DELETE_MAILBOX);
	db->mailbox_delete_contents = database_prepare(log, db->conn, DELETE_MAILBOX_CONTENTS);
	db->fetch = database_prepare(log, db->conn, FETCH_MAIL);

	db->selection_exists = database_prepare(log, db->conn, SELECTION_EXISTS);
	db->inbox_exists = database_prepare(log, db->conn, INBOX_EXISTS);
	db->selection_recent = database_prepare(log, db->conn, SELECTION_RECENT);
	db->inbox_recent = database_prepare(log, db->conn, INBOX_RECENT);

	if(is_master){
		db->query_userdatabase = database_prepare(log, db->conn, QUERY_USER_DATABASE);

		if(!db->query_userdatabase){
			logprintf(log, LOG_ERROR, "Failed to prepare user database location query\n");
			return -1;
		}
	}

	if(!db->mailbox_find){
		logprintf(log, LOG_ERROR, "Failed to prepare mailbox search query\n");
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
		.mailbox_create = NULL,
		.mailbox_delete = NULL,
		.fetch = NULL,
		.query_userdatabase = NULL,

		.selection_exists = NULL,
		.inbox_exists = NULL,
		.selection_recent = NULL,
		.inbox_recent = NULL
	};

	if(db->conn){
		sqlite3_finalize(db->mailbox_find);
		sqlite3_finalize(db->mailbox_create);
		sqlite3_finalize(db->mailbox_delete);
		sqlite3_finalize(db->fetch);
		sqlite3_finalize(db->query_userdatabase);

		sqlite3_finalize(db->selection_exists);
		sqlite3_finalize(db->inbox_exists);
		sqlite3_finalize(db->selection_recent);
		sqlite3_finalize(db->inbox_recent);

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
