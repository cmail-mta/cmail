int maildrop_read(LOGGER log, sqlite3_stmt* stmt, MAILDROP* maildrop, char* user_name, bool is_master){
	int status=0;
	unsigned rows=maildrop->count;
	unsigned index=maildrop->count;
	unsigned i;
	POP_MAIL empty_mail = {
		.database_id = 0,
		.mail_size = 0,
		.flag_master = is_master,
		.flag_delete = false
	};

	if(sqlite3_bind_text(stmt, 1, user_name, -1, SQLITE_STATIC) == SQLITE_OK){
		do{
			status=sqlite3_step(stmt);
			switch(status){
				case SQLITE_ROW:
					if(!index>=maildrop->count){
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

					index++;
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

int maildrop_acquire(LOGGER log, DATABASE* database, MAILDROP* maildrop, char* user_name){
	int status=0;

	//lock maildrop
	if(maildrop_lock(log, database, user_name, true)<0){
		return -1;
	}

	//read mail data from master
	if(maildrop_read(log, database->list_master, maildrop, user_name, true)<0){
		logprintf(log, LOG_WARNING, "Failed to read master maildrop for user %s\n", user_name);
		status=-1;
	}
	
	//create user table statements if needed
	//TODO

	//read mail data from user table if needed
	//TODO
	
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
	if(maildrop_lock(log, database, user_name, false)<0){
		logprintf(log, LOG_WARNING, "Failed to unlock maildrop for user %s\n", user_name);
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
	}

	//free maildrop data
	if(maildrop->mails){
		free(maildrop->mails);
	}

	*maildrop=empty_maildrop;
	
	return status;
}
