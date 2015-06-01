int maildrop_read(LOGGER log, sqlite3_stmt* stmt, MAILDROP* maildrop, char* user_name, bool is_master){
	int status=0;
	char* message_id;
	unsigned rows=maildrop->count;
	unsigned index=maildrop->count;
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
			status=sqlite3_step(stmt);
			switch(status){
				case SQLITE_ROW:
					if(index>=maildrop->count){
						//expand the maildrop
						rows+=CMAIL_MAILDROP_CHUNK;
						maildrop->mails=realloc(maildrop->mails, rows*sizeof(POP_MAIL));
						for(i=index;i<rows;i++){
							maildrop->mails[i]=empty_mail;
						}
						maildrop->count=rows;
					}

					if(index>=maildrop->count){
						logprintf(log, LOG_WARNING, "Maildrop reading went out of bounds, this should not have happened\n");
						break;
					}

					maildrop->mails[index].database_id=sqlite3_column_int(stmt, 0);
					maildrop->mails[index].mail_size=sqlite3_column_int(stmt, 1);
					message_id=(char*)sqlite3_column_text(stmt, 2);
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
		while(status==SQLITE_ROW);
		maildrop->count=index;
	}
	else{
		logprintf(log, LOG_WARNING, "Failed to bind mail query parameter\n");
		status=-1;
	}

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);
	return status;
}

int maildrop_lock(LOGGER log, DATABASE* database, char* user_name, bool lock){
	int status;

	//atomically modify maildrop lock, bail out if it fails
	if(sqlite3_bind_int(database->update_lock, 1, lock?1:0) != SQLITE_OK
			|| sqlite3_bind_text(database->update_lock, 2, user_name, -1, SQLITE_STATIC) !=SQLITE_OK
			|| sqlite3_bind_int(database->update_lock, 3, lock?0:1) != SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind lock update parameter\n");
		sqlite3_reset(database->update_lock);
		sqlite3_clear_bindings(database->update_lock);
		return -1;
	}

	status=sqlite3_step(database->update_lock);
	switch(status){
		case SQLITE_DONE:
			status=0;
			//check if lock was updated
			if(sqlite3_changes(database->conn)!=1){
				status=-1;
			}
			break;
		default:
			logprintf(log, LOG_INFO, "Unhandled return value from lock update: %d\n", status);
			status=1;
	}

	sqlite3_reset(database->update_lock);
	sqlite3_clear_bindings(database->update_lock);

	return status;
}

int maildrop_user_attach(LOGGER log, DATABASE* database, MAILDROP* maildrop, char* user_name){
	int status=1;
	char* dbfile;

	char* LIST_MAILS_USER="SELECT mail_id, length(mail_data) AS length, mail_ident FROM %s.mailbox WHERE mail_user=? ORDER BY mail_submission ASC;";
	char* FETCH_MAIL_USER="SELECT mail_data FROM %s.mailbox WHERE mail_id=?;";
	char* DELETE_MAIL_USER="DELETE FROM %s.mailbox WHERE mail_id=?;";

	char* list_user=calloc(strlen(LIST_MAILS_USER)+strlen(user_name)+1, sizeof(char));
	char* fetch_user=calloc(strlen(FETCH_MAIL_USER)+strlen(user_name)+1, sizeof(char));
	char* delete_user=calloc(strlen(DELETE_MAIL_USER)+strlen(user_name)+1, sizeof(char));

	if(!list_user || !fetch_user || !delete_user){
		logprintf(log, LOG_ERROR, "Failed to allocate memory for statement text\n");
		return -1;
	}

	snprintf(list_user, strlen(LIST_MAILS_USER)+strlen(user_name), LIST_MAILS_USER, user_name);
	snprintf(fetch_user, strlen(FETCH_MAIL_USER)+strlen(user_name), FETCH_MAIL_USER, user_name);
	snprintf(delete_user, strlen(DELETE_MAIL_USER)+strlen(user_name), DELETE_MAIL_USER, user_name);

	//test for user databases
	if(sqlite3_bind_text(database->query_userdatabase, 1, user_name, -1, SQLITE_STATIC) == SQLITE_OK){
		switch(sqlite3_step(database->query_userdatabase)){
			case SQLITE_ROW:
				//attach user database
				dbfile=(char*)sqlite3_column_text(database->query_userdatabase, 0);
				logprintf(log, LOG_INFO, "User %s has user database %s\n", user_name, dbfile);

				if(sqlite3_bind_text(database->db_attach, 1, dbfile, -1, SQLITE_STATIC) == SQLITE_OK
					&& sqlite3_bind_text(database->db_attach, 2, user_name, -1, SQLITE_STATIC) == SQLITE_OK){
					switch(sqlite3_step(database->db_attach)){
						case SQLITE_CANTOPEN:
							logprintf(log, LOG_ERROR, "Cannot open database %s\n", dbfile);
							status=-1;
							break;
						case SQLITE_DONE:
							logprintf(log, LOG_INFO, "User database %s attached for user %s\n", dbfile, user_name);

							//create user table statements
							maildrop->list_user=database_prepare(log, database->conn, list_user);
							maildrop->fetch_user=database_prepare(log, database->conn, fetch_user);
							maildrop->delete_user=database_prepare(log, database->conn, delete_user);

							if(!maildrop->list_user || !maildrop->fetch_user || !maildrop->delete_user){
								logprintf(log, LOG_WARNING, "Failed to prepare mail management statement for user database\n");
								status=-1;
							}
							else{
								status=0;
							}
							break;
						case SQLITE_ERROR:
						default:
							status=-1;
							logprintf(log, LOG_ERROR, "Failed to attach database: %s\n", sqlite3_errmsg(database->conn));
							break;
					}
				}
				else{
					logprintf(log, LOG_WARNING, "Failed to bind attach parameters\n");
					status=-1;
				}

				sqlite3_reset(database->db_attach);
				sqlite3_clear_bindings(database->db_attach);
				break;
			case SQLITE_DONE:
				//no user database, done here
				break;
			default:
				logprintf(log, LOG_WARNING, "Failed to query user database for %s: %s\n", user_name, sqlite3_errmsg(database->conn));
				status=-1;
				break;
		}
	}
	else{
		logprintf(log, LOG_WARNING, "Failed to bind user parameter to user database query\n");
		status=-1;
	}

	sqlite3_reset(database->query_userdatabase);
	sqlite3_clear_bindings(database->query_userdatabase);

	free(list_user);
	free(fetch_user);
	free(delete_user);

	return status;
}

int maildrop_acquire(LOGGER log, DATABASE* database, MAILDROP* maildrop, char* user_name){
	int status=0;

	//lock maildrop
	if(maildrop_lock(log, database, user_name, true)<0){
		logprintf(log, LOG_WARNING, "Maildrop for user %s could not be locked\n", user_name);
		return -1;
	}

	//read mail data from master
	if(maildrop_read(log, database->list_master, maildrop, user_name, true)<0){
		logprintf(log, LOG_WARNING, "Failed to read master maildrop for user %s\n", user_name);
		status=-1;
	}

	switch(maildrop_user_attach(log, database, maildrop, user_name)){
		case 1:
			logprintf(log, LOG_INFO, "User %s does not have a user database\n", user_name);
			break;
		case 0:
			//read mail data from user database
			if(maildrop_read(log, maildrop->list_user, maildrop, user_name, false)<0){
				logprintf(log, LOG_WARNING, "Failed to read user maildrop for %s\n", user_name);
				status=-1;
			}
			break;
		default:
			logprintf(log, LOG_WARNING, "Failed to attach database for user %s\n", user_name);
			status=-1;
	}

	logprintf(log, LOG_INFO, "Maildrop for user %s is at %d mails\n", user_name, maildrop->count);
	return status;
}

int maildrop_delete(LOGGER log, sqlite3_stmt* stmt, unsigned mail){
	int status=0;

	if(sqlite3_bind_int(stmt, 1, mail) == SQLITE_OK){
		status=sqlite3_step(stmt);
		switch(status){
			case SQLITE_DONE:
				status=0;
				break;
			default:
				logprintf(log, LOG_WARNING, "Unhandled response code when deleting mail (%d)\n", status);
				status=-1;
				break;
		}
	}
	else{
		logprintf(log, LOG_WARNING, "Failed to bind mail id to deletion statement\n");
		status=-1;
	}

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);
	return status;
}

int maildrop_update(LOGGER log, DATABASE* database, MAILDROP* maildrop){
	unsigned i;
	int status=0;

	for(i=0;i<maildrop->count;i++){
		if(maildrop->mails[i].flag_delete){
			//delete this mail
			if(maildrop->mails[i].flag_master){
				//delete from master
				if(maildrop_delete(log, database->delete_master, maildrop->mails[i].database_id)<0){
					status=-1;
				}
			}
			else{
				//delete from user database
				if(maildrop_delete(log, maildrop->delete_user, maildrop->mails[i].database_id)<0){
					status=-1;
				}
			}
		}
	}

	return status;
}

int maildrop_release(LOGGER log, DATABASE* database, MAILDROP* maildrop, char* user_name){
	MAILDROP empty_maildrop = {
		.count = 0,
		.mails = NULL,
		.list_user = NULL,
		.fetch_user = NULL,
		.delete_user = NULL
	};
	int status=0;

	//lock maildrop
	if(user_name){
		if(maildrop_lock(log, database, user_name, false)<0){
			logprintf(log, LOG_WARNING, "Failed to unlock maildrop for user %s\n", user_name);
		}
	}

	//free user statements
	if(maildrop->list_user){
		sqlite3_finalize(maildrop->list_user);
	}
	if(maildrop->fetch_user){
		sqlite3_finalize(maildrop->fetch_user);
	}
	if(maildrop->delete_user){
		sqlite3_finalize(maildrop->delete_user);

		//detach user database
		if(sqlite3_bind_text(database->db_detach, 1, user_name, -1, SQLITE_STATIC) == SQLITE_OK){
			switch(sqlite3_step(database->db_detach)){
				case SQLITE_DONE:
					logprintf(log, LOG_INFO, "User database for %s detached\n", user_name);
					break;
				default:
					logprintf(log, LOG_WARNING, "Failed to detach user database %s\n", user_name);
					break;
			}
		}
		else{
			logprintf(log, LOG_WARNING, "Failed to bind detach statement parameter\n");
		}

		sqlite3_reset(database->db_detach);
		sqlite3_clear_bindings(database->db_detach);
	}

	//free maildrop data
	if(maildrop->mails){
		free(maildrop->mails);
	}

	*maildrop=empty_maildrop;

	return status;
}
