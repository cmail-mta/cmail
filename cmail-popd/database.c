int database_initialize(LOGGER log, DATABASE* database){
	char* QUERY_AUTHENTICATION_DATA="SELECT user_authdata FROM main.users WHERE user_name = ?;";
	char* QUERY_USER_DATABASE="SELECT user_database FROM main.users WHERE user_name = ? AND user_database NOT NULL;";
	char* UPDATE_POP_LOCK="UPDATE OR FAIL main.popd SET pop_lock=? WHERE pop_user=? AND pop_lock=?;";
	char* LIST_MAILS_MASTER="SELECT mail_id, length(mail_data) AS length, mail_ident FROM main.mailbox WHERE mail_user=? ORDER BY mail_submission ASC;";
	char* FETCH_MAIL_MASTER="SELECT mail_data FROM main.mailbox WHERE mail_id=?;";
	char* DELETE_MAIL_MASTER="DELETE FROM main.mailbox WHERE mail_id=?;";

	char* QUERY_ATTACH_DB="ATTACH DATABASE ? AS ?;";
	char* QUERY_DETACH_DB="DETACH DATABASE ?;";
	
	//check the database schema version
	if(database_schema_version(log, database->conn)!=CMAIL_CURRENT_SCHEMA_VERSION){
		logprintf(log, LOG_ERROR, "The database schema is at another version than required for this build\n");
		return -1;
	}

	database->query_authdata=database_prepare(log, database->conn, QUERY_AUTHENTICATION_DATA);
	database->query_userdatabase=database_prepare(log, database->conn, QUERY_USER_DATABASE);
	database->update_lock=database_prepare(log, database->conn, UPDATE_POP_LOCK);
	database->list_master=database_prepare(log, database->conn, LIST_MAILS_MASTER);
	database->fetch_master=database_prepare(log, database->conn, FETCH_MAIL_MASTER);
	database->delete_master=database_prepare(log, database->conn, DELETE_MAIL_MASTER);
	
	database->db_attach=database_prepare(log, database->conn, QUERY_ATTACH_DB);
	database->db_detach=database_prepare(log, database->conn, QUERY_DETACH_DB);

	if(!database->db_attach || !database->db_detach){
		logprintf(log, LOG_ERROR, "Failed to compile attach/detach queries\n");
		return -1;
	}

	if(!database->query_authdata || !database->query_userdatabase){
		logprintf(log, LOG_ERROR, "Failed to prepare user data query\n");
		return -1;
	}

	if(!database->update_lock){
		logprintf(log, LOG_ERROR, "Failed to prepare lock modification statement\n");
		return -1;
	}

	if(!database->list_master || !database->fetch_master || !database->delete_master){
		logprintf(log, LOG_ERROR, "Failed to prepare mail data query\n");
		return -1;
	}

	return 0;
}

void database_free(LOGGER log, DATABASE* database){
	//FIXME check for SQLITE_BUSY here
	
	if(database->conn){
		sqlite3_finalize(database->query_authdata);
		sqlite3_finalize(database->query_userdatabase);
		sqlite3_finalize(database->update_lock);
		sqlite3_finalize(database->list_master);
		sqlite3_finalize(database->fetch_master);
		sqlite3_finalize(database->delete_master);

		sqlite3_finalize(database->db_attach);
		sqlite3_finalize(database->db_detach);

		sqlite3_close(database->conn);
		database->conn=NULL;
	}
}
