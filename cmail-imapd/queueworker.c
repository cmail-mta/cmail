int queueworker_arbitrate_command(LOGGER log, QUEUED_COMMAND* entry, WORKER_CLIENT* client){
	unsigned i;

	logprintf(log, LOG_DEBUG, "Handling command [%s]: %s\n", entry->tag, entry->command);
	if(entry->parameters){
		for(i = 0; entry->parameters[i] > 0; i++){
			logprintf(log, LOG_DEBUG, "Command parameter %d: %s\n", i, entry->backing_buffer + entry->parameters[i]);
		}
	}

	if(!strcasecmp(entry->command, "noop")){
		//TODO check for new mail
		return 0;
	}
	else if(!strcasecmp(entry->command, "xyzzy")){
		//round-trip xyzzy
		entry->replies = common_strappf(entry->replies, &(entry->replies_length),
				"* XYZZY Round-trip completed\r\n%s OK Incantation completed\r\n", entry->tag);
		if(!entry->replies){
			//FIXME this is really bad and probably should be adressed by falling back to a static failure response buffer
			logprintf(log, LOG_ERROR, "Failed to reallocate command reply\n");
		}
		return 0;
	}

	//FIXME need to be able to increase failscore here
	return -1;
}

int queueworker_release_client(LOGGER log, WORKER_CLIENT* client, bool data_valid){
	WORKER_CLIENT empty = {
		.client = NULL,
		.user_database = {
			.conn = NULL,
			.mailbox_find = NULL,
			.mailbox_info = NULL,
			.mailbox_create = NULL,
			.mailbox_delete = NULL,
			.query_userdatabase = NULL,
			.fetch = NULL
		},
		.authorized_user = NULL,
		.selection_master = 0,
		.selection_user = 0,
		.select_readwrite = false
	};

	if(data_valid){
		free(client->authorized_user);
		database_free_worker(&(client->user_database));
	}

	*client = empty;
	return 0;
}

int queueworker_acquire_client(LOGGER log, WORKER_DATABASE* master, CONNECTION* client, WORKER_CLIENT* worker_client){
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

void* queueworker_coreloop(void* param){
	THREAD_CONFIG* thread_config = (THREAD_CONFIG*) param;
	LOGGER log = thread_config->log;
	COMMAND_QUEUE* queue = thread_config->queue;
	QUEUED_COMMAND* head = NULL;
	unsigned entries_done = 0, i;
	int last_slot;
	WORKER_CLIENT client_data[CMAIL_MAX_CONCURRENT_CLIENTS];

	WORKER_DATABASE master = {
		.conn = NULL,
		.mailbox_find = NULL,
		.mailbox_info = NULL,
		.mailbox_create = NULL,
		.mailbox_delete = NULL,
		.query_userdatabase = NULL,
		.fetch = NULL
	};

	if(database_init_worker(log, (char*)thread_config->master_db, &master, true) < 0){
		logprintf(log, LOG_ERROR, "Failed to open master database in queue worker\n");
		database_free_worker(&master);
		abort_signaled = 1;
	}

	//initialize per-client data
	for(i = 0; i < CMAIL_MAX_CONCURRENT_CLIENTS; i++){
		queueworker_release_client(log, client_data + i, false);
	}

	pthread_mutex_lock(&(queue->queue_access));
	logprintf(log, LOG_INFO, "Queue worker entering main loop\n");
	while(!abort_signaled){
		logprintf(log, LOG_DEBUG, "Queue worker running queue\n");

		for(head = queue->head; head; head = head->next){
			//log_dump_buffer(log, LOG_DEBUG, head, sizeof(QUEUED_COMMAND));
			switch(head->queue_state){
				case COMMAND_NEW:
					//process queued command
					head->queue_state = COMMAND_IN_PROGRESS;

					//check for local client data
					last_slot = -1;
					for(i = 0; i < CMAIL_MAX_CONCURRENT_CLIENTS; i++){
						if(client_data[i].client == head->client){
							break;
						}
						else if(last_slot < 0 && !client_data[i].client){
							last_slot = i;
						}
					}

					if(i >= CMAIL_MAX_CONCURRENT_CLIENTS){
						if(last_slot >= 0){
							if(queueworker_acquire_client(log, &master, head->client, client_data + last_slot) < 0){
								logprintf(log, LOG_ERROR, "Failed to gather client data for background processing\n");
								//FIXME return static error buffer
								head->queue_state = COMMAND_INTERNAL_FAILURE;
								break;
							}
							i = last_slot;
						}
						else{
							//should never happen
							logprintf(log, LOG_ERROR, "Ran out of worker client slots\n");
							//FIXME return static error buffer as response
							head->queue_state = COMMAND_INTERNAL_FAILURE;
							break;
						}
					}

					pthread_mutex_unlock(&(queue->queue_access));

					//do the actual work
					if(queueworker_arbitrate_command(log, head, client_data + i) < 0){
						logprintf(log, LOG_WARNING, "Command execution returned error in queue worker\n");
					}

					entries_done++;
					pthread_mutex_lock(&(queue->queue_access));
					head->queue_state = COMMAND_REPLY;
					break;
				case COMMAND_SYSTEM:
					logprintf(log, LOG_DEBUG, "Queue worker handled system message\n");
					//release worker client data
					if(head->discard && head->client){
						for(i = 0; i < CMAIL_MAX_CONCURRENT_CLIENTS; i++){
							if(client_data[i].client == head->client){
								queueworker_release_client(log, client_data + i, true);
							}
						}
					}
					entries_done++;
					head->queue_state = COMMAND_REPLY;
					break;
				case COMMAND_CANCELED:
					//canceled commands need to round-trip through the worker because the head pointer is thread-local
					head->queue_state = COMMAND_CANCEL_ACK;
					break;
				default:
					break;
			}
		}

		logprintf(log, LOG_DEBUG, "Queue run handled %d items\n", entries_done);
		if(entries_done > 0){
			//notify feedback pipe
			//FIXME might want to check the return value here
			write(thread_config->feedback_pipe[1], "c", 1);
		}
		entries_done = 0;
		pthread_cond_wait(&(queue->queue_dirty), &(queue->queue_access));
	}

	pthread_mutex_unlock(&(queue->queue_access));

	logprintf(log, LOG_DEBUG, "Queue worker shutting down\n");

	//release all allocated data
	for(i = 0; i < CMAIL_MAX_CONCURRENT_CLIENTS; i++){
		if(client_data[i].client){
			queueworker_release_client(log, client_data + i, true);
		}
	}
	database_free_worker(&master);

	return 0;
}
