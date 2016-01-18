int queueworker_arbitrate_command(LOGGER log, WORKER_DATABASE* master, QUEUED_COMMAND* entry, WORKER_CLIENT* client){
	unsigned i;
	int rv = 0;

	logprintf(log, LOG_DEBUG, "Handling command [%s]: %s\n", entry->tag, entry->command);
	if(entry->parameters){
		for(i = 0; entry->parameters[i] > 0; i++){
			logprintf(log, LOG_DEBUG, "Command parameter %d: %s\n", i, entry->backing_buffer + entry->parameters[i]);
		}
	}

	if(!strcasecmp(entry->command, "authenticate") || !strcasecmp(entry->command, "login")){
		entry->replies = common_strappf(entry->replies, &(entry->replies_length),
				"%s OK Access granted\r\n", entry->tag);
	}
	else if(!strcasecmp(entry->command, "noop")){
		//TODO check for new mail
		rv = -1;
	}
	else if(entry->parameters && !strcasecmp(entry->command, "create")){
		rv = imap_create(log, master, entry, client->authorized_user, entry->backing_buffer + entry->parameters[0]);
		if(rv >= 0 && client->user_database.conn){
			rv = imap_create(log, &(client->user_database), entry, client->authorized_user, entry->backing_buffer + entry->parameters[0]);
			if(rv < 0){
				//FIXME roll back changes to master database
			}
		}

		if(rv == 0){
			entry->replies = common_strappf(entry->replies, &(entry->replies_length),
					"%s OK Mailbox created\r\n", entry->tag);
		}
		else if(entry->replies && entry->replies[0]){
			rv = 0;
		}
	}
	else if(entry->parameters && !strcasecmp(entry->command, "delete")){
		//TODO implement mailbox deletion
	}
	else if(entry->parameters && (!strcasecmp(entry->command, "examine") || !strcasecmp(entry->command, "select"))){
		client->selection_master = database_resolve_path(log, master, client->authorized_user, entry->backing_buffer + entry->parameters[0], NULL);
		if(client->selection_master < 0){
			//FIXME this should probably detect internal errors
			entry->replies = common_strappf(entry->replies, &(entry->replies_length),
					"%s NO Failed to select mailbox\r\n", entry->tag);
			rv = 0;
		}
		else{
			if(client->user_database.conn){
				client->selection_user = database_resolve_path(log, &(client->user_database), client->authorized_user, entry->backing_buffer + entry->parameters[0], NULL);
				rv = (client->selection_user < 0) ? -1:0;
			}

			if(rv == 0){
				//select implies readwrite access
				client->select_readwrite = (entry->command[0] == 's') ? true:false;

				logprintf(log, LOG_DEBUG, "Client selected mailbox %d @ master, %d @ user for readwrite %s\n", client->selection_master, client->selection_user, client->select_readwrite ? "true":"false");
				//TODO actually send required data back to client
				entry->replies = common_strappf(entry->replies, &(entry->replies_length),
						"%s OK Selection now active\r\n", entry->tag);
				rv = 0;
			}
			else{
				client->selection_master = -1;
				client->selection_user = -1;
				entry->replies = common_strappf(entry->replies, &(entry->replies_length),
						"%s NO Failed to select mailbox\r\n", entry->tag);
				rv = 0;
			}
		}
	}
	else if(entry->parameters && !strcasecmp(entry->command, "subscribe")){
	}
	else if(entry->parameters && !strcasecmp(entry->command, "unsubscribe")){
	}
	else if(!strcasecmp(entry->command, "xyzzy")){
		//round-trip xyzzy
		logprintf(log, LOG_DEBUG, "User database: %s\n", client->user_database.conn ? "attached":"none");
		logprintf(log, LOG_DEBUG, "Selection: master %d, user %d, readwrite %s\n", client->selection_master, client->selection_user, client->select_readwrite ? "true":"false");
		entry->replies = common_strappf(entry->replies, &(entry->replies_length),
				"* XYZZY Round-trip completed\r\n%s OK Incantation completed\r\n", entry->tag);
	}
	else{
		//default branch, command not known but enqueued
		rv = -1;
	}

	if(!entry->replies){
		logprintf(log, LOG_ERROR, "Failed to allocate command reply or no reply generated\n");
		rv = -1;
	}

	//returning -1 here automatically generates a COMMAND_INTERNAL_ERROR
	return rv;
}


void* queueworker_coreloop(void* param){
	THREAD_CONFIG* thread_config = (THREAD_CONFIG*) param;
	LOGGER log = thread_config->log;
	COMMAND_QUEUE* queue = thread_config->queue;
	QUEUED_COMMAND* head = NULL;
	unsigned entries_done = 0, i;
	WORKER_CLIENT client_data[CMAIL_MAX_CONCURRENT_CLIENTS];
	WORKER_CLIENT* current_client = NULL;

	WORKER_DATABASE master;
	IMAP_STATE current_state;

	//reset entries
	master.conn = NULL;
 	database_free_worker(&master);	

	//attach master
	if(database_init_worker(log, (char*)thread_config->master_db, &master, true) < 0){
		logprintf(log, LOG_ERROR, "Failed to open master database in queue worker\n");
		database_free_worker(&master);
		abort_signaled = 1;
	}

	//initialize per-client data
	for(i = 0; i < CMAIL_MAX_CONCURRENT_CLIENTS; i++){
		client_data[i].user_database.conn = NULL;
		workerdata_release(log, client_data + i, false);
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
					entries_done++;

					//check for local client data
					current_client = workerdata_get(log, client_data, &master, head->client);
					if(!current_client){
						head->queue_state = COMMAND_INTERNAL_FAILURE;
						break;
					}

					pthread_mutex_unlock(&(queue->queue_access));

					//do the actual work
					current_state = COMMAND_REPLY;
					if(queueworker_arbitrate_command(log, &master, head, current_client) < 0){
						logprintf(log, LOG_WARNING, "Command execution returned error in queue worker\n");
						current_state = COMMAND_INTERNAL_FAILURE;
					}

					pthread_mutex_lock(&(queue->queue_access));
					head->queue_state = current_state;
					break;
				case COMMAND_SYSTEM:
					logprintf(log, LOG_DEBUG, "Queue worker handled system message\n");
					//release worker client data
					if(head->discard && head->client){
						for(i = 0; i < CMAIL_MAX_CONCURRENT_CLIENTS; i++){
							if(client_data[i].client == head->client){
								workerdata_release(log, client_data + i, true);
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
			workerdata_release(log, client_data + i, true);
		}
	}
	database_free_worker(&master);

	return 0;
}
