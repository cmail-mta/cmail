int database_attach(LOGGER log, DATABASE* database, char* dbfile){
	char* INSERT_USER_MAILBOX = "INSERT INTO mailbox (mail_user, mail_ident, mail_envelopeto, mail_envelopefrom, mail_submitter, mail_proto, mail_data) VALUES (?, ?, ?, ?, ?, ?, ?);";
	unsigned slot;
	USER_DATABASE* entry;

	//initialize user storage structure
	if(!database->mail_storage.users){
		database->mail_storage.users = calloc(1, sizeof(USER_DATABASE*));
		if(!database->mail_storage.users){
			logprintf(log, LOG_ERROR, "Failed to allocate memory for user storage database structure\n");
			return -1;
		}
	}

	//create/reuse entry in user storage structure
	for(slot = 0; database->mail_storage.users[slot]; slot++){
		if(!database->mail_storage.users[slot]->conn){
			break;
		}
	}

	//if no usable slot, reallocate
	if(!database->mail_storage.users[slot]){
		database->mail_storage.users = realloc(database->mail_storage.users, (slot + 2) * sizeof(USER_DATABASE*));
		if(!database->mail_storage.users){
			logprintf(log, LOG_ERROR, "Failed to reallocate user storage database structure\n");
			return -1;
		}
		database->mail_storage.users[slot + 1] = NULL;
		database->mail_storage.users[slot] = calloc(1, sizeof(USER_DATABASE));
		if(!database->mail_storage.users[slot]){
			logprintf(log, LOG_ERROR, "Failed to allocate user storage database structure\n");
			return -1;
		}
	}

	entry = database->mail_storage.users[slot];
	entry->conn = database_open(log, dbfile, SQLITE_OPEN_READWRITE);
	if(!entry->conn){
		//FIXME this might need some more error handling
		//TODO Return the slot, etc
		return -1;
	}

	entry->mailbox = database_prepare(log, entry->conn, INSERT_USER_MAILBOX);
	if(!entry->mailbox){
		logprintf(log, LOG_ERROR, "Failed to create user mailbox insert query\n");
		//FIXME handle error
		return -1;
	}

	entry->file_name = common_strdup(dbfile);
	if(!entry->file_name){
		logprintf(log, LOG_ERROR, "Failed to copy user storage file name\n");
		//FIXME better error handling
		return -1;
	}

	return 0;
}

int database_detach(LOGGER log, USER_DATABASE* db){
	USER_DATABASE empty_db = {
		.conn = NULL,
		.active = false,
		.mailbox = NULL,
		.file_name = NULL
	};

	//TODO error check these
	sqlite3_finalize(db->mailbox);
	sqlite3_close(db->conn);
	free(db->file_name);

	*db = empty_db;

	return 0;
}

USER_DATABASE* database_userdb(LOGGER log, DATABASE* database, char* filename){
	unsigned i;

	if(!database->mail_storage.users){
		return NULL;
	}

	for(i = 0; database->mail_storage.users[i]; i++){
		if(database->mail_storage.users[i]->file_name && !strcmp(database->mail_storage.users[i]->file_name, filename)){
			return database->mail_storage.users[i];
		}
	}

	logprintf(log, LOG_INFO, "User storage queried for unknown database %s\n", filename);
	return NULL;
}

int database_refresh(LOGGER log, DATABASE* database){
	int status, rv = 0;
	unsigned i;

	char* QUERY_USER_DATABASES = "SELECT user_database FROM main.users WHERE user_database NOT NULL GROUP BY user_database;";

	sqlite3_stmt* select_dbs = database_prepare(log, database->conn, QUERY_USER_DATABASES);
	if(!select_dbs){
		logprintf(log, LOG_ERROR, "Failed to prepare user storage management statement\n");
		return -1;
	}

	if(database->mail_storage.users){
		for(i = 0; database->mail_storage.users[i]; i++){
			database->mail_storage.users[i]->active = false;
		}
	}

	do{
		//fetch user database
		status = sqlite3_step(select_dbs);
		switch(status){
			case SQLITE_ROW:
				//if not attached, attach
				if(!database_userdb(log, database, (char*)sqlite3_column_text(select_dbs, 0))){
					//attach
					if(database_attach(log, database, (char*)sqlite3_column_text(select_dbs, 0)) < 0){
						logprintf(log, LOG_ERROR, "Failed to attach database: %s\n", sqlite3_errmsg(database->conn));
						status = SQLITE_ERROR;
						rv = -1;
					}
					else{
						//database seems to have been attached ok, mark it active
						database_userdb(log, database, (char*)sqlite3_column_text(select_dbs, 0))->active = true;
					}
				}
				break;
			case SQLITE_DONE:
				//traversed all databases
				break;
			default:
				logprintf(log, LOG_ERROR, "User storage database initialization failed: %s\n", sqlite3_errmsg(database->conn));
				rv = -1;
				break;
		}
	}
	while(status == SQLITE_ROW);

	//detach inactive databases
	if(database->mail_storage.users){
		for(i = 0; database->mail_storage.users[i]; i++){
			if(!database->mail_storage.users[i]->active){
				//FIXME check this for return value
				database_detach(log, database->mail_storage.users[i]);
			}
		}
	}

	sqlite3_finalize(select_dbs);
	return rv;
}

int database_initialize(LOGGER log, DATABASE* database){
	char* QUERY_ADDRESS_INFO = "SELECT address_router, address_route FROM main.addresses WHERE ? LIKE address_expression ORDER BY address_order DESC;";
	char* INSERT_MASTER_MAILBOX = "INSERT INTO main.mailbox (mail_user, mail_ident, mail_envelopeto, mail_envelopefrom, mail_submitter, mail_proto, mail_data) VALUES (?, ?, ?, ?, ?, ?, ?);";
	char* INSERT_MASTER_OUTBOX = "INSERT INTO main.outbox (mail_remote, mail_envelopefrom, mail_envelopeto, mail_submitter, mail_data) VALUES (?, ?, ?, ?, ?);";
	char* QUERY_ORIGINATING_ROUTER = "SELECT smtpd_router, smtpd_route FROM main.smtpd WHERE smtpd_user = ?;";

	//this query has to conform to the auth_validate contract (the first 2 columns are fixed)
	char* QUERY_AUTHENTICATION_DATA = "SELECT user_authdata, user_alias, user_database FROM main.users WHERE user_name = ?;";

	//check the database schema version
	if(database_schema_version(log, database->conn)!=CMAIL_CURRENT_SCHEMA_VERSION){
		logprintf(log, LOG_ERROR, "The database schema is at another version than required for this build\n");
		return -1;
	}

	database->query_authdata = database_prepare(log, database->conn, QUERY_AUTHENTICATION_DATA);
	database->query_address = database_prepare(log, database->conn, QUERY_ADDRESS_INFO);
	database->query_outbound_router = database_prepare(log, database->conn, QUERY_ORIGINATING_ROUTER);
	database->mail_storage.mailbox_master = database_prepare(log, database->conn, INSERT_MASTER_MAILBOX);
	database->mail_storage.outbox_master = database_prepare(log, database->conn, INSERT_MASTER_OUTBOX);

	if(!database->query_authdata){
		logprintf(log, LOG_ERROR, "Failed to prepare authentication data query\n");
		return -1;
	}

	if(!database->query_address){
		logprintf(log, LOG_ERROR, "Failed to prepare address query statement\n");
		return -1;
	}

	if(!database->query_outbound_router){
		logprintf(log, LOG_ERROR, "Failed to prepare outbound router query statement\n");
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
		sqlite3_finalize(database->query_address);
		sqlite3_finalize(database->query_outbound_router);
		sqlite3_finalize(database->mail_storage.mailbox_master);
		sqlite3_finalize(database->mail_storage.outbox_master);

		//finalize user storage
		if(database->mail_storage.users){
			for(i = 0; database->mail_storage.users[i]; i++){
				//FIXME user return value
				database_detach(log, database->mail_storage.users[i]);

				free(database->mail_storage.users[i]);
			}
			free(database->mail_storage.users);
		}

		sqlite3_close(database->conn);
		database->conn = NULL;
	}
}
