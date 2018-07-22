int maildrop_read(sqlite3_stmt* stmt, MAILDROP* maildrop, char* user_name, bool is_master){
	int status = 0;
	char* message_id;
	unsigned rows = maildrop->count;
	unsigned index = maildrop->count;
	unsigned i;

	POP_MAIL empty_mail = {
		.database_id = 0,
		.mail_size = 0,
		.flag_master = is_master,
		.flag_delete = false,
		.message_id = ""
	};

	if(sqlite3_bind_text(stmt, 1, user_name, -1, SQLITE_STATIC) == SQLITE_OK){
		do{
			status = sqlite3_step(stmt);
			switch(status){
				case SQLITE_ROW:
					if(index >= maildrop->count){
						//expand the maildrop
						rows += CMAIL_MAILDROP_CHUNK;
						maildrop->mails = realloc(maildrop->mails, rows * sizeof(POP_MAIL));
						for(i = index; i < rows; i++){
							maildrop->mails[i] = empty_mail;
						}
						maildrop->count = rows;
					}

					if(index >= maildrop->count){
						logprintf(LOG_WARNING, "Maildrop reading went out of bounds, this should not have happened\n");
						break;
					}

					maildrop->mails[index].database_id = sqlite3_column_int(stmt, 0);
					maildrop->mails[index].mail_size = sqlite3_column_int(stmt, 1);
					message_id = (char*)sqlite3_column_text(stmt, 2);
					if(message_id){
						strncpy(maildrop->mails[index].message_id, message_id, POP_MESSAGEID_MAX);
					}

					index++;
					break;
				case SQLITE_ERROR:
					//FIXME handle this
					break;
			}
		}
		while(status == SQLITE_ROW);
		maildrop->count = index;
	}
	else{
		logprintf(LOG_WARNING, "Failed to bind mail query parameter\n");
		status = -1;
	}

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);
	return status;
}

int maildrop_lock(DATABASE* database, char* user_name, bool lock){
	int status;

	//atomically modify maildrop lock, bail out if it fails
	if(sqlite3_bind_int(database->update_lock, 1, lock ? 1:0) != SQLITE_OK
			|| sqlite3_bind_text(database->update_lock, 2, user_name, -1, SQLITE_STATIC) != SQLITE_OK
			|| sqlite3_bind_int(database->update_lock, 3, lock?0:1) != SQLITE_OK){
		logprintf(LOG_ERROR, "Failed to bind lock update parameter\n");
		sqlite3_reset(database->update_lock);
		sqlite3_clear_bindings(database->update_lock);
		return -1;
	}

	status = sqlite3_step(database->update_lock);
	switch(status){
		case SQLITE_DONE:
			status = 0;
			//check if lock was updated
			if(sqlite3_changes(database->conn) != 1){
				status = -1;
			}
			break;
		default:
			logprintf(LOG_INFO, "Unhandled return value from lock update: %d\n", status);
			status = 1;
	}

	sqlite3_reset(database->update_lock);
	sqlite3_clear_bindings(database->update_lock);

	return status;
}

int maildrop_user_attach(DATABASE* database, MAILDROP* maildrop, char* user_name){
	int status = 1;
	char* dbfile = NULL;
	char* err_str = NULL;

	char* LIST_MAILS_USER = "SELECT mail_id, length(mail_data) AS length, mail_ident FROM main.mailbox WHERE mail_user=? ORDER BY mail_submission ASC;";
	char* FETCH_MAIL_USER = "SELECT mail_data FROM main.mailbox WHERE mail_id=?;";

	char* MARK_DELETION = "INSERT INTO temp.deletions (user, mail) VALUES (?, ?);";
	char* UNMARK_DELETIONS = "DELETE FROM temp.deletions WHERE user = ?;";
	char* DELETE_MAIL_USER = "DELETE FROM main.mailbox WHERE mail_id IN (SELECT mail FROM temp.deletions WHERE user = ?);";

	char* CREATE_DELETION_TABLE = "CREATE TEMPORARY TABLE temp.deletions (user TEXT NOT NULL, mail INTEGER NOT NULL);";

	//test for user databases
	if(sqlite3_bind_text(database->query_userdatabase, 1, user_name, -1, SQLITE_STATIC) == SQLITE_OK){
		switch(sqlite3_step(database->query_userdatabase)){
			case SQLITE_ROW:
				//attach user database
				dbfile = (char*)sqlite3_column_text(database->query_userdatabase, 0);
				logprintf(LOG_INFO, "User %s has user database %s\n", user_name, dbfile);

				maildrop->user_conn = database_open(dbfile, SQLITE_OPEN_READWRITE);

				if(!(maildrop->user_conn)){
					status = -1;
					logprintf(LOG_ERROR, "Failed to attach user database %s\n", dbfile);
					break;
				}

				logprintf(LOG_INFO, "User database %s attached for user %s\n", dbfile, user_name);

				//create the temp table to store deletions
				switch(sqlite3_exec(maildrop->user_conn, CREATE_DELETION_TABLE, NULL, NULL, &err_str)){
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

				//create user table statements
				maildrop->list_user = database_prepare(maildrop->user_conn, LIST_MAILS_USER);
				maildrop->fetch_user = database_prepare(maildrop->user_conn, FETCH_MAIL_USER);
				maildrop->mark_deletion = database_prepare(maildrop->user_conn, MARK_DELETION);
				maildrop->unmark_deletions = database_prepare(maildrop->user_conn, UNMARK_DELETIONS);
				maildrop->delete_user = database_prepare(maildrop->user_conn, DELETE_MAIL_USER);

				if(!maildrop->list_user || !maildrop->fetch_user || !maildrop->delete_user || !maildrop->mark_deletion || !maildrop->unmark_deletions){
					logprintf(LOG_WARNING, "Failed to prepare user mail access statements\n");
					status = -1;
				}
				else{
					status = 0;
				}
				break;
			case SQLITE_DONE:
				//no user database, done here
				break;
			default:
				logprintf(LOG_WARNING, "Failed to query user database for %s: %s\n", user_name, sqlite3_errmsg(database->conn));
				status = -1;
				break;
		}
	}
	else{
		logprintf(LOG_WARNING, "Failed to bind user parameter to user database query\n");
		status = -1;
	}

	sqlite3_reset(database->query_userdatabase);
	sqlite3_clear_bindings(database->query_userdatabase);

	return status;
}

int maildrop_acquire(DATABASE* database, MAILDROP* maildrop, char* user_name){
	int status = 0;

	//lock maildrop
	if(maildrop_lock(database, user_name, true) < 0){
		logprintf(LOG_WARNING, "Maildrop for user %s could not be locked\n", user_name);
		return -1;
	}

	//read mail data from master
	if(maildrop_read(database->list_master, maildrop, user_name, true) < 0){
		logprintf(LOG_WARNING, "Failed to read master maildrop for user %s\n", user_name);
		status = -1;
	}

	switch(maildrop_user_attach(database, maildrop, user_name)){
		case 1:
			logprintf(LOG_INFO, "User %s does not have a user database\n", user_name);
			break;
		case 0:
			//read mail data from user database
			if(maildrop_read(maildrop->list_user, maildrop, user_name, false) < 0){
				logprintf(LOG_WARNING, "Failed to read user maildrop for %s\n", user_name);
				status = -1;
			}
			break;
		default:
			logprintf(LOG_WARNING, "Failed to attach database for user %s\n", user_name);
			status = -1;
	}

	//FIXME unlock the maildrop on error (if it was not locked beforehand)

	logprintf(LOG_INFO, "Maildrop for user %s is at %d mails\n", user_name, maildrop->count);
	return status;
}

int maildrop_mark(DATABASE* database, char* user_name, MAILDROP* maildrop, int mail_id){
	int rv = 0;
	sqlite3_stmt* mark_deletion = maildrop->mails[mail_id].flag_master ? database->mark_deletion:maildrop->mark_deletion;

	if(sqlite3_bind_text(mark_deletion, 1, user_name, -1, SQLITE_STATIC) == SQLITE_OK
			&& sqlite3_bind_int(mark_deletion, 2, maildrop->mails[mail_id].database_id) == SQLITE_OK){
		switch(sqlite3_step(mark_deletion)){
			case SQLITE_DONE:
			case SQLITE_OK:
				logprintf(LOG_DEBUG, "Marked mail %d %s as deleted (user %s)\n", mail_id, maildrop->mails[mail_id].flag_master? "in master":"in userdb", user_name);
				break;
			default:
				logprintf(LOG_WARNING, "Failed to mark mail as deleted: %s\n", sqlite3_errmsg(maildrop->mails[mail_id].flag_master ? database->conn:maildrop->user_conn));
				rv = -1;
				break;
		}
	}
	else{
		logprintf(LOG_ERROR, "Failed to bind parameter to deletion mark query\n");
		rv = -1;
	}

	sqlite3_reset(mark_deletion);
	sqlite3_clear_bindings(mark_deletion);
	return rv;
}

int maildrop_unmark(sqlite3* conn, sqlite3_stmt* unmark_deletions, char* user_name){
	int rv = 0;

	if(sqlite3_bind_text(unmark_deletions, 1, user_name, -1, SQLITE_STATIC) == SQLITE_OK){
		switch(sqlite3_step(unmark_deletions)){
			case SQLITE_DONE:
			case SQLITE_OK:
				logprintf(LOG_DEBUG, "Deletions table for %s cleared (%d marks deleted)\n", user_name, sqlite3_changes(conn));
				break;
			default:
				logprintf(LOG_WARNING, "Failed to clear deletion table: %s\n", sqlite3_errmsg(conn));
				rv = -1;
				break;
		}
	}
	else{
		logprintf(LOG_ERROR, "Failed to bind deletion user name parameter\n");
		rv = -1;
	}

	sqlite3_reset(unmark_deletions);
	sqlite3_clear_bindings(unmark_deletions);

	return rv;
}

int maildrop_delete(sqlite3_stmt* deletion_stmt, char* user_name){
	int rv = 0;

	if(sqlite3_bind_text(deletion_stmt, 1, user_name, -1, SQLITE_STATIC) == SQLITE_OK){
		switch(sqlite3_step(deletion_stmt)){
			case SQLITE_DONE:
			case SQLITE_OK:
				break;
			default:
				logprintf(LOG_WARNING, "Failed to perform deletion\n");
				rv = -1;
				break;
		}
	}
	else{
		logprintf(LOG_ERROR, "Failed to bind deletion user name parameter\n");
		rv = -1;
	}

	sqlite3_reset(deletion_stmt);
	sqlite3_clear_bindings(deletion_stmt);
	return rv;
}

int maildrop_update(DATABASE* database, MAILDROP* maildrop, char* user_name){
	int status = 0;
	logprintf(LOG_INFO, "Performing deletions\n");

	//delete mails from master
	if(maildrop_delete(database->delete_master, user_name) < 0){
		logprintf(LOG_ERROR, "Failed to delete marked mails from master database: %s\n", sqlite3_errmsg(database->conn));
		status = -1;
	}

	logprintf(LOG_INFO, "Deleted %d mails from master database\n", sqlite3_changes(database->conn));

	//delete mails from user database if present
	if(maildrop->user_conn){
		if(maildrop_delete(maildrop->delete_user, user_name) < 0){
			logprintf(LOG_ERROR, "Failed to delete marked mails from user database: %s\n", sqlite3_errmsg(maildrop->user_conn));
			status = -1;
		}

		logprintf(LOG_INFO, "Deleted %d mails from user database\n", sqlite3_changes(maildrop->user_conn));
	}
	else{
		logprintf(LOG_DEBUG, "Not deleting from user database, none attached\n");
	}

	return status;
}

int maildrop_release(DATABASE* database, MAILDROP* maildrop, char* user_name){
	MAILDROP empty_maildrop = {
		.count = 0,
		.mails = NULL,
		.user_conn = NULL,
		.list_user = NULL,
		.fetch_user = NULL,
		.mark_deletion = NULL,
		.unmark_deletions = NULL,
		.delete_user = NULL
	};
	int status = 0;

	//reset master deletion tables
	if(maildrop_unmark(database->conn, database->unmark_deletions, user_name) < 0){
		logprintf(LOG_ERROR, "Failed to reset master deletion table\n");
		status = -1;
	}

	//reset user deletion table
	if(maildrop->user_conn && maildrop_unmark(maildrop->user_conn, maildrop->unmark_deletions, user_name) < 0){
		logprintf(LOG_ERROR, "Failed to reset user database deletion table\n");
		status = -1;
	}

	//unlock maildrop
	if(maildrop_lock(database, user_name, false) < 0){
		logprintf(LOG_WARNING, "Failed to unlock maildrop for user %s\n", user_name);
	}

	//free user statements
	if(maildrop->user_conn){
		sqlite3_finalize(maildrop->list_user);
		sqlite3_finalize(maildrop->fetch_user);
		sqlite3_finalize(maildrop->delete_user);
		sqlite3_finalize(maildrop->mark_deletion);
		sqlite3_finalize(maildrop->unmark_deletions);
		sqlite3_close(maildrop->user_conn);
	}

	//free maildrop data
	if(maildrop->mails){
		free(maildrop->mails);
	}

	*maildrop = empty_maildrop;

	return status;
}
