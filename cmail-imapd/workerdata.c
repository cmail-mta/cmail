int workerdata_release(LOGGER log, WORKER_CLIENT* client, bool data_valid){
	WORKER_CLIENT empty = {
		.client = NULL,
		.user_database = {
			.conn = NULL
		},
		.authorized_user = NULL,
		.selection_master = 0,
		.selection_user = 0,
		.select_readwrite = false
	};

	if(data_valid){
		free(client->authorized_user);
	}
	//do this in any case to reset the database entries
	database_free_worker(&(client->user_database));

	*client = empty;
	return 0;
}

int workerdata_acquire(LOGGER log, WORKER_DATABASE* master, CONNECTION* client, WORKER_CLIENT* worker_client){
	CLIENT* client_data = (CLIENT*)client->aux_data;
	int rv = -1;
	char* dbfile = NULL;

	//copy over authorized user
	worker_client->authorized_user = common_strdup(client_data->auth.user.authorized);
	if(!worker_client->authorized_user){
		logprintf(log, LOG_ERROR, "Failed to allocate memory for user name\n");
		return -1;
	}

	//query user database location
	if(sqlite3_bind_text(master->query_userdatabase, 1, worker_client->authorized_user, -1, SQLITE_STATIC) == SQLITE_OK){
		switch(sqlite3_step(master->query_userdatabase)){
			case SQLITE_ROW:
				//attach user database
				dbfile = (char*)sqlite3_column_text(master->query_userdatabase, 0);
				logprintf(log, LOG_INFO, "User %s has user database %s\n", worker_client->authorized_user, dbfile);

				if(database_init_worker(log, dbfile, &(worker_client->user_database), false) < 0){
					logprintf(log, LOG_ERROR, "Failed to open user database for %s in queue worker\n", worker_client->authorized_user);
					database_free_worker(&(worker_client->user_database));
				}
				else{
					rv = 0;
				}
				break;
			case SQLITE_DONE:
				//no user database
				logprintf(log, LOG_INFO, "User %s has no user database\n", worker_client->authorized_user);
				rv = 0;
				break;
			default:
				logprintf(log, LOG_WARNING, "Failed to query for user database for user %s: %s\n", worker_client->authorized_user, sqlite3_errmsg(master->conn));
		}
	}
	else{
		logprintf(log, LOG_ERROR, "Failed to bind user name parameter to user database query\n");
	}

	sqlite3_reset(master->query_userdatabase);
	sqlite3_clear_bindings(master->query_userdatabase);

	if(rv == 0){
		worker_client->client = client;
	}
	else{
		free(worker_client->authorized_user);
	}

	return rv;
}

WORKER_CLIENT* workerdata_get(LOGGER log, WORKER_CLIENT* clients, WORKER_DATABASE* master, CONNECTION* client){
	int last_slot = -1;
	size_t i;

	for(i = 0; i < CMAIL_MAX_CONCURRENT_CLIENTS; i++){
		if(clients[i].client == client){
			break;
		}
		else if(last_slot < 0 && !clients[i].client){
			last_slot = i;
		}
	}

	if(i >= CMAIL_MAX_CONCURRENT_CLIENTS){
		if(last_slot >= 0){
			if(workerdata_acquire(log, master, client, clients + last_slot) < 0){
				logprintf(log, LOG_ERROR, "Failed to gather client data for background processing\n");
				return NULL;
			}
			i = last_slot;
		}
		else{
			//should never happen
			logprintf(log, LOG_ERROR, "Ran out of worker client slots\n");
			return NULL;
		}
	}

	return clients + i;
}
