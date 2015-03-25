int database_attach(LOGGER log, DATABASE* database, sqlite3_stmt* attach, char* dbfile, char* name){
	char* INSERT_USER_MAILBOX="INSERT INTO %s.mailbox (mail_user, mail_envelopeto, mail_envelopefrom, mail_submitter, mail_data) VALUES (?, ?, ?, ?, ?);";
	char* user_stmt=NULL;
	int status, rv=0;
	unsigned slot;
	USER_DATABASE* entry;

	//initialize user storage structure
	if(!database->mail_storage.users){
		database->mail_storage.users=calloc(1, sizeof(USER_DATABASE*));
		if(!database->mail_storage.users){
			logprintf(log, LOG_ERROR, "Failed to allocate memory for user storage database structure\n");
			return -1;
		}
	}

	//create/reuse entry in user storage structure	
	for(slot=0;database->mail_storage.users[slot];slot++){
		if(!database->mail_storage.users[slot]->file_name){
			break;
		}
	}

	//if no usable slot, reallocate
	if(!database->mail_storage.users[slot]){
		database->mail_storage.users=realloc(database->mail_storage.users, (slot+2)*sizeof(USER_DATABASE*));
		if(!database->mail_storage.users){
			logprintf(log, LOG_ERROR, "Failed to reallocate user storage database structure\n");
			return -1;
		}
		database->mail_storage.users[slot+1]=NULL;
		database->mail_storage.users[slot]=calloc(1, sizeof(USER_DATABASE));
		if(!database->mail_storage.users[slot]){
			logprintf(log, LOG_ERROR, "Failed to allocate user storage database structure\n");
			return -1;
		}
	}

	entry=database->mail_storage.users[slot];

	if(sqlite3_bind_text(attach, 1, dbfile, -1, SQLITE_STATIC)==SQLITE_OK
			&& sqlite3_bind_text(attach, 2, name, -1, SQLITE_STATIC)==SQLITE_OK){
		status=sqlite3_step(attach);

		switch(status){
			case SQLITE_CANTOPEN:
				logprintf(log, LOG_ERROR, "Database %s does not exist\n", dbfile);
				rv=-1;
				break;
			case SQLITE_DONE:
				logprintf(log, LOG_INFO, "Attached database %s as %s\n", dbfile, name);

				//create statement
				user_stmt=calloc(strlen(INSERT_USER_MAILBOX)+strlen(name)+2, sizeof(char));
				if(!user_stmt){
					logprintf(log, LOG_ERROR, "Failed to allocate temporary statement storage\n");
					rv=-1;
					break;
				}

				snprintf(user_stmt, strlen(INSERT_USER_MAILBOX)+strlen(name)+1, INSERT_USER_MAILBOX, name);
				entry->mailbox=database_prepare(log, database->conn, user_stmt);
				free(user_stmt);

				if(!entry->mailbox){
					logprintf(log, LOG_ERROR, "Failed to create user mailbox insert query\n");
					rv=-1;
					break;
				}

				//fill entry
				entry->file_name=calloc(strlen(dbfile)+1, sizeof(char));
				entry->conn_handle=calloc(strlen(name)+1, sizeof(char));

				if(!entry->file_name || !entry->conn_handle){
					logprintf(log, LOG_ERROR, "Failed to allocate user storage structure members\n");
					rv=-1;
					break;
				}
				strncpy(entry->file_name, dbfile, strlen(dbfile));
				strncpy(entry->conn_handle, name, strlen(name));
				break;
			case SQLITE_ERROR:
				rv=-1;
				break;
			default:
				logprintf(log, LOG_WARNING, "Uncaught attach response code %d\n", status);
				rv=1;
				break;
		}
	}
	else{
		logprintf(log, LOG_ERROR, "Failed to bind attach statement parameter\n");
		rv=-1;
	}

	sqlite3_reset(attach);
	sqlite3_clear_bindings(attach);
	return rv;
}

int database_detach(LOGGER log, DATABASE* database, sqlite3_stmt* detach, USER_DATABASE* db){
	int status, rv=0;
	USER_DATABASE empty_db = {
		.active=false,
		.mailbox = NULL,
		.file_name = NULL,
		.conn_handle = NULL
	};
	
	if(detach){
		if(sqlite3_bind_text(detach, 1, db->conn_handle, -1, SQLITE_STATIC)==SQLITE_OK){
			status=sqlite3_step(detach);

			switch(status){
				case SQLITE_DONE:
					logprintf(log, LOG_INFO, "Detached database %s\n", db->conn_handle);
					//FIXME check result
					break;
				case SQLITE_ERROR:
					rv=-1;
					break;
				default:
					logprintf(log, LOG_WARNING, "Uncaught detach response code %d\n", status);
					rv=1;
					break;
			}
		}
		else{
			logprintf(log, LOG_ERROR, "Failed to bind detach statement parameter\n");
			rv=-1;
		}

		sqlite3_reset(detach);
		sqlite3_clear_bindings(detach);
	}
	
	sqlite3_finalize(db->mailbox);
	free(db->file_name);
	free(db->conn_handle);

	*db=empty_db;

	return rv;
}

USER_DATABASE* database_userdb(LOGGER log, DATABASE* database, char* filename){
	unsigned i;

	if(!database->mail_storage.users){
		return NULL;
	}

	for(i=0;database->mail_storage.users[i];i++){
		if(database->mail_storage.users[i]->file_name && !strcmp(database->mail_storage.users[i]->file_name, filename)){
			return database->mail_storage.users[i];
		}
	}

	logprintf(log, LOG_INFO, "User storage queried for unknown database %s\n", filename);
	return NULL;
}

int database_refresh(LOGGER log, DATABASE* database){
	int status, rv=0;
	unsigned i;
	
	char* QUERY_ATTACH_DB="ATTACH DATABASE ? AS ?;";
	char* QUERY_DETACH_DB="DETACH DATABASE ?;";
	char* QUERY_USER_DATABASES="SELECT MIN(msa_user), msa_inroute FROM main.msa WHERE msa_inrouter='store' AND msa_inroute NOT NULL GROUP BY msa_inroute;";
	
	sqlite3_stmt* attach_db=database_prepare(log, database->conn, QUERY_ATTACH_DB);
	sqlite3_stmt* detach_db=database_prepare(log, database->conn, QUERY_DETACH_DB);
	sqlite3_stmt* select_dbs=database_prepare(log, database->conn, QUERY_USER_DATABASES);
	
	if(!attach_db||!detach_db||!select_dbs){
		logprintf(log, LOG_ERROR, "Failed to prepare user storage management statements\n");
		return -1;
	}

	if(database->mail_storage.users){
		for(i=0;database->mail_storage.users[i];i++){
			database->mail_storage.users[i]->active=false;
		}
	}
	
	do{
		//fetch user database
		status=sqlite3_step(select_dbs);
		switch(status){
			case SQLITE_ROW:
				//if not attached, attach
				if(!database_userdb(log, database, (char*)sqlite3_column_text(select_dbs, 1))){
					//attach
					switch(database_attach(log, database, attach_db, (char*)sqlite3_column_text(select_dbs, 1), (char*)sqlite3_column_text(select_dbs, 0))){
						case -1:
							logprintf(log, LOG_ERROR, "Failed to attach database: %s\n", sqlite3_errmsg(database->conn));
							status=SQLITE_ERROR;
							rv=-1;
							break;
						case 1:
							logprintf(log, LOG_ERROR, "Additional Information: %s\n", sqlite3_errmsg(database->conn));
						default:
							//database seems to have been attached ok, mark it active
							database_userdb(log, database, (char*)sqlite3_column_text(select_dbs, 1))->active=true;
							break;
					}
				}
				break;
			case SQLITE_DONE:
				//traversed all databases
				break;
			default:
				logprintf(log, LOG_ERROR, "User storage database initialization failed: %s\n", sqlite3_errmsg(database->conn));
				rv=-1;
				break;
		}
	}
	while(status==SQLITE_ROW);

	//detach inactive databases
	if(database->mail_storage.users){
		for(i=0;database->mail_storage.users[i];i++){
			if(!database->mail_storage.users[i]->active){
				//FIXME check this for return value
				database_detach(log, database, detach_db, database->mail_storage.users[i]);
			}
		}
	}

	sqlite3_finalize(attach_db);
	sqlite3_finalize(detach_db);
	sqlite3_finalize(select_dbs);
	return rv;
}

int database_initialize(LOGGER log, DATABASE* database){
	char* QUERY_ADDRESS_USER="SELECT address_user, msa_inrouter, msa_outrouter FROM main.addresses JOIN main.msa ON address_user = msa_user WHERE ? LIKE address_expression ORDER BY address_order DESC;";
	char* QUERY_USER_ROUTER_INBOUND="SELECT msa_inrouter, msa_inroute FROM main.msa WHERE msa_user = ?;";
	char* QUERY_USER_ROUTER_OUTBOUND="SELECT msa_outrouter, msa_outroute FROM main.msa WHERE msa_user = ?;";
	char* INSERT_MASTER_MAILBOX="INSERT INTO main.mailbox (mail_user, mail_envelopeto, mail_envelopefrom, mail_submitter, mail_data) VALUES (?, ?, ?, ?, ?);";
	char* INSERT_MASTER_OUTBOX="INSERT INTO main.outbox (mail_remote, mail_envelopefrom, mail_envelopeto, mail_submitter, mail_data) VALUES (?, ?, ?, ?, ?);";
	char* QUERY_AUTHENTICATION_DATA="SELECT user_authdata FROM main.users WHERE user_name = ?;";
	
	database->query_authdata=database_prepare(log, database->conn, QUERY_AUTHENTICATION_DATA);
	database->query_addresses=database_prepare(log, database->conn, QUERY_ADDRESS_USER);
	database->query_inrouter=database_prepare(log, database->conn, QUERY_USER_ROUTER_INBOUND);
	database->query_outrouter=database_prepare(log, database->conn, QUERY_USER_ROUTER_OUTBOUND);
	database->mail_storage.mailbox_master=database_prepare(log, database->conn, INSERT_MASTER_MAILBOX);
	database->mail_storage.outbox_master=database_prepare(log, database->conn, INSERT_MASTER_OUTBOX);

	if(!database->query_authdata){
		logprintf(log, LOG_ERROR, "Failed to prepare authentication data query\n");
		return -1;
	}

	if(!database->query_addresses){
		logprintf(log, LOG_ERROR, "Failed to prepare address query statement\n");
		return -1;
	}

	if(!database->query_inrouter || !database->query_outrouter){
		logprintf(log, LOG_ERROR, "Failed to prepare router query statement\n");
		return -1;
	}

	if(!database->mail_storage.mailbox_master || !database->mail_storage.outbox_master){
		logprintf(log, LOG_ERROR, "Failed to prepare mail storage statement\n");
		return -1;
	}

	return database_refresh(log, database);
}

void database_free(LOGGER log, DATABASE* database){
	unsigned i;

	//FIXME check for SQLITE_BUSY here
	if(database->conn){
		sqlite3_finalize(database->query_authdata);
		sqlite3_finalize(database->query_addresses);
		sqlite3_finalize(database->query_inrouter);
		sqlite3_finalize(database->query_outrouter);
		sqlite3_finalize(database->mail_storage.mailbox_master);
		sqlite3_finalize(database->mail_storage.outbox_master);

		//finalize user storage
		if(database->mail_storage.users){
			for(i=0;database->mail_storage.users[i];i++){
				//FIXME user return value
				database_detach(log, database, NULL, database->mail_storage.users[i]);

				free(database->mail_storage.users[i]);
			}
			free(database->mail_storage.users);
		}
		
		sqlite3_close(database->conn);
		database->conn=NULL;
	}
}
