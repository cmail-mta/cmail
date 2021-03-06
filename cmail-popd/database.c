int database_initialize(DATABASE* database){
	char* QUERY_AUTHENTICATION_DATA = "SELECT user_authdata, user_alias FROM main.users WHERE user_name = ?;";
	char* QUERY_USER_DATABASE = "SELECT user_database FROM main.users WHERE user_name = ? AND user_database NOT NULL;";
	char* UPDATE_POP_LOCK = "UPDATE OR FAIL main.popd SET pop_lock=? WHERE pop_user = ? AND pop_lock = ?;";

	char* LIST_MAILS_MASTER = "SELECT mail_id, length(mail_data) AS length, mail_ident FROM main.mailbox WHERE mail_user = ? ORDER BY mail_submission ASC;";
	char* FETCH_MAIL_MASTER = "SELECT mail_data FROM main.mailbox WHERE mail_id = ?;";

	char* MARK_DELETION = "INSERT INTO temp.deletions (user, mail) VALUES (?, ?);";
	char* UNMARK_DELETIONS = "DELETE FROM temp.deletions WHERE user = ?;";
	char* DELETE_MAIL_MASTER = "DELETE FROM main.mailbox WHERE mail_id IN (SELECT mail FROM temp.deletions WHERE user = ?);";

	char* CREATE_DELETION_TABLE = "CREATE TEMPORARY TABLE temp.deletions (user TEXT NOT NULL, mail INTEGER NOT NULL);";

	char* err_str = NULL;

	//check the database schema version
	if(database_schema_version(database->conn) != CMAIL_CURRENT_SCHEMA_VERSION){
		logprintf(LOG_ERROR, "The database schema is at another version than required for this build\n");
		return -1;
	}

	//create the temp table to store deletions
	switch(sqlite3_exec(database->conn, CREATE_DELETION_TABLE, NULL, NULL, &err_str)){
		case SQLITE_OK:
		case SQLITE_DONE:
			break;
		default:
			logprintf(LOG_WARNING, "Non-completion response to temp table create statement\n");
	}

	if(err_str){
		logprintf(LOG_ERROR, "Failed to create temporary deletion table: %s\n", err_str);
		sqlite3_free(err_str);
		return -1;
	}

	database->query_authdata = database_prepare(database->conn, QUERY_AUTHENTICATION_DATA);
	database->query_userdatabase = database_prepare(database->conn, QUERY_USER_DATABASE);
	database->update_lock = database_prepare(database->conn, UPDATE_POP_LOCK);
	database->list_master = database_prepare(database->conn, LIST_MAILS_MASTER);
	database->fetch_master = database_prepare(database->conn, FETCH_MAIL_MASTER);

	database->mark_deletion = database_prepare(database->conn, MARK_DELETION);
	database->unmark_deletions = database_prepare(database->conn, UNMARK_DELETIONS);
	database->delete_master = database_prepare(database->conn, DELETE_MAIL_MASTER);

	if(!database->query_authdata || !database->query_userdatabase){
		logprintf(LOG_ERROR, "Failed to prepare user data query\n");
		return -1;
	}

	if(!database->update_lock){
		logprintf(LOG_ERROR, "Failed to prepare lock modification statement\n");
		return -1;
	}

	if(!database->list_master || !database->fetch_master){
		logprintf(LOG_ERROR, "Failed to prepare mail data query\n");
		return -1;
	}

	if(!database->mark_deletion || !database->unmark_deletions || !database->delete_master){
		logprintf(LOG_ERROR, "Failed to prepare some deletion statements\n");
		return -1;
	}

	return 0;
}

void database_free(DATABASE* database){
	//FIXME check for SQLITE_BUSY here

	if(database->conn){
		sqlite3_finalize(database->query_authdata);
		sqlite3_finalize(database->query_userdatabase);
		sqlite3_finalize(database->update_lock);
		sqlite3_finalize(database->list_master);
		sqlite3_finalize(database->fetch_master);

		sqlite3_finalize(database->mark_deletion);
		sqlite3_finalize(database->unmark_deletions);
		sqlite3_finalize(database->delete_master);

		sqlite3_close(database->conn);
		database->conn = NULL;
	}
}
