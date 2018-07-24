int imap_selection_count(WORKER_DATABASE* master, WORKER_CLIENT* client, char* filter_flag){
	if(client->selection_master < 0){
		logprintf(LOG_ERROR, "Could not fetch selection count: no selection\n");
		//FIXME this might end up in some output
		return -1;
	}

	int count_master = 0;
	int count_user = 0;

	if(!filter_flag){
		//all mails in current selection
		//FIXME this assumes that if one selection is 0/inbox, both are.
		//this might break if someone choses to play around with the default database contents
		if(client->selection_master == 0){
			//inbox selection count
			if(database_aggregate_query(&count_master, master->inbox_exists, 1, ARG_STRING, client->authorized_user) < 0 
					|| (client->user_database.conn && database_aggregate_query(&count_user, client->user_database.inbox_exists, 1, ARG_STRING, client->authorized_user) < 0)){
				logprintf(LOG_ERROR, "Failed to query EXISTS count for user %s INBOX\n", client->authorized_user);
				//FIXME safe return value?
				return 0;
			}
		}
		else{
			//mailbox selection count
			if(database_aggregate_query(&count_master, master->selection_exists, 1, ARG_INTEGER, client->selection_master) < 0 
					|| (client->user_database.conn && database_aggregate_query(&count_user, client->user_database.selection_exists, 1, ARG_INTEGER, client->selection_user) < 0)){
				logprintf(LOG_ERROR, "Failed to query EXISTS count for user %s mailbox %d/%d\n", client->authorized_user, client->selection_master, client->selection_user);
				//FIXME safe return value?
				return 0;
			}
		}

		return count_master + count_user;
	}

	if(!strcmp(filter_flag, "\\Recent")){
		//mails without flags in current selection
		//TODO this needs to support mails actively flagged RECENT
		//FIXME this assumes that if one selection is 0/inbox, both are.
		//this might break if someone choses to play around with the default database contents
		if(client->selection_master == 0){
			//inbox selection count
			if(database_aggregate_query(&count_master, master->inbox_recent, 1, ARG_STRING, client->authorized_user) < 0 
					|| (client->user_database.conn && database_aggregate_query(&count_user, client->user_database.inbox_recent, 1, ARG_STRING, client->authorized_user) < 0)){
				logprintf(LOG_ERROR, "Failed to query RECENT count for user %s INBOX\n", client->authorized_user);
				//FIXME safe return value?
				return 0;
			}
		}
		else{
			//mailbox selection count
			if(database_aggregate_query(&count_master, master->selection_recent, 1, ARG_INTEGER, client->selection_master) < 0 
					|| (client->user_database.conn && database_aggregate_query(&count_user, client->user_database.selection_recent, 1, ARG_INTEGER, client->selection_user) < 0)){
				logprintf(LOG_ERROR, "Failed to query RECENT count for user %s mailbox %d/%d\n", client->authorized_user, client->selection_master, client->selection_user);
				//FIXME safe return value?
				return 0;
			}
		}

		return count_master + count_user;
	}

	//mails with filter_flag set
	//FIXME is this needed?
	logprintf(LOG_DEBUG, "Implementation dead end hit\n"); //TODO
	return 0;
}
