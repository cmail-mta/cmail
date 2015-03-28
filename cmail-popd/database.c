int database_initialize(LOGGER log, DATABASE* database){
	char* QUERY_AUTHENTICATION_DATA="SELECT user_authdata FROM main.users WHERE user_name = ?;";
	char* UPDATE_POP_LOCK="UPDATE OR FAIL main.popd SET pop_lock=? WHERE pop_user=? AND pop_lock=?;";
	char* LIST_MAILS_MASTER="SELECT mail_id, length(mail_data) AS length FROM main.mailbox WHERE mail_user=?;";
	char* FETCH_MAIL_MASTER="SELECT mail_data FROM main.mailbox WHERE mail_id=?;";
	char* DELETE_MAIL_MASTER="DELETE FROM main.mailbox WHERE mail_id=?;";

	database->query_authdata=database_prepare(log, database->conn, QUERY_AUTHENTICATION_DATA);
	database->update_lock=database_prepare(log, database->conn, UPDATE_POP_LOCK);
	database->list_master=database_prepare(log, database->conn, LIST_MAILS_MASTER);
	database->fetch_master=database_prepare(log, database->conn, FETCH_MAIL_MASTER);
	database->delete_master=database_prepare(log, database->conn, DELETE_MAIL_MASTER);
	
	if(!database->query_authdata){
		logprintf(log, LOG_ERROR, "Failed to prepare authentication data query\n");
		return -1;
	}

	if(!database->update_lock){
		logprintf(log, LOG_ERROR, "Failed to prepare lock fetch/update statements\n");
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
		sqlite3_finalize(database->update_lock);
		sqlite3_finalize(database->list_master);
		sqlite3_finalize(database->fetch_master);
		sqlite3_finalize(database->delete_master);

		sqlite3_close(database->conn);
		database->conn=NULL;
	}
}
