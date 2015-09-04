//this code queries outbound routing information for a user
MAILROUTE route_query(LOGGER log, DATABASE* database, char* user){
	MAILROUTE route = {
		.router = NULL,
		.argument = NULL
	};
	int status;

	if(!user){
		return route;
	}

	status = sqlite3_bind_text(database->query_outbound_router, 1, user, -1, SQLITE_STATIC);
	if(status != SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind user parameter to routing query: %s\n", sqlite3_errmsg(database->conn));
		sqlite3_reset(database->query_outbound_router);
		sqlite3_clear_bindings(database->query_outbound_router);
		return route;
	}

	switch(sqlite3_step(database->query_outbound_router)){
		case SQLITE_ROW:
			//copy data
			//This works because router is NOT NULL in schema
			route.router = common_strdup((char*)sqlite3_column_text(database->query_outbound_router, 0));

			if(sqlite3_column_text(database->query_outbound_router, 1)){
				route.argument = common_strdup((char*)sqlite3_column_text(database->query_outbound_router, 1));
			}

			if(!route.router){
				logprintf(log, LOG_ERROR, "Failed to allocate memory for ROUTE structure\n");
			}
			break;
		case SQLITE_DONE:
			logprintf(log, LOG_ERROR, "Database contains no outbound router definition for %s\n", user);
			break;
		default:
			logprintf(log, LOG_WARNING, "Unhandled query return value: %s\n", sqlite3_errmsg(database->conn));
			break;
	}

	sqlite3_reset(database->query_outbound_router);
	sqlite3_clear_bindings(database->query_outbound_router);

	return route;
}

void route_free(MAILROUTE* route){
	if(route){
		if(route->router){
			free(route->router);
			route->router = NULL;
		}

		if(route->argument){
			free(route->argument);
			route->argument = NULL;
		}
	}
}

int route_local_path(LOGGER log, DATABASE* database, MAIL* mail, MAILPATH* current_path){
	int rv = 0;
	USER_DATABASE* user_db;
	char path_replacement[SMTP_MAX_PATH_LENGTH];
	char forward_path[SMTP_MAX_PATH_LENGTH];

	//reject the path if the path does not have a router table entry
	//this check should be redundant, as paths without routers are non-local
	if(!current_path->route.router){
		return -1;
	}

	logprintf(log, LOG_DEBUG, "Inbound router %s (%s) for %s\n", current_path->route.router, current_path->route.argument ? current_path->route.argument:"none", current_path->path);

	if(!strcmp(current_path->route.router, "store")){
		if(!(current_path->route.argument)){
			logprintf(log, LOG_ERROR, "Path is assigned store router without argument, rejecting transaction\n");
			//Fail the transaction permanently
			rv = -1;
		}
		else{
			//preemptively fail the transaction...
			rv = -1;

			//check for user existence (as well as a user database path)
			if(sqlite3_bind_text(database->query_authdata, 1, current_path->route.argument, -1, SQLITE_STATIC) == SQLITE_OK){
				switch(sqlite3_step(database->query_authdata)){
					case SQLITE_ROW:
						//check for a user database
						if(sqlite3_column_text(database->query_authdata, 2)){
							logprintf(log, LOG_DEBUG, "Fetching user database for user %s (%s)\n", current_path->route.argument, (char*)sqlite3_column_text(database->query_authdata, 2));	
							user_db = database_userdb(log, database, (char*)sqlite3_column_text(database->query_authdata, 2));

							if(!user_db){
								//try to refresh the user database set
								database_refresh(log, database);
								user_db = database_userdb(log, database, (char*)sqlite3_column_text(database->query_authdata, 2));

								if(!user_db){
									//as last resort, store to master db
									logprintf(log, LOG_WARNING, "Stored mail for user %s to master instead of defined database\n", current_path->route.argument);
									rv = mail_store_inbox(log, database->mail_storage.mailbox_master, mail, current_path);
								}
								else{
									logprintf(log, LOG_DEBUG, "Storing mail for %s to user database %s after database refresh\n", current_path->route.argument, user_db->file_name);
									rv = mail_store_inbox(log, user_db->mailbox, mail, current_path);
								}
							}
							else{
								logprintf(log, LOG_DEBUG, "Storing mail for %s to user database %s\n", current_path->route.argument, user_db->file_name);
								rv = mail_store_inbox(log, user_db->mailbox, mail, current_path);
							}

						}
						else{
							//simply store to master
							logprintf(log, LOG_DEBUG, "Storing mail for %s to master database\n", current_path->route.argument);
							rv = mail_store_inbox(log, database->mail_storage.mailbox_master, mail, current_path);
						}
						break;
					case SQLITE_DONE:
						logprintf(log, LOG_ERROR, "Failed to resolve router argument %s to a user, failing transaction\n", current_path->route.argument);
						break;
					default:
						logprintf(log, LOG_ERROR, "Failed to execute user info query: %s\n", sqlite3_errmsg(database->conn));
						break;
				}
			}
			else{
				logprintf(log, LOG_ERROR, "Failed to bind user to user info query\n");
			}

			sqlite3_reset(database->query_authdata);
			sqlite3_clear_bindings(database->query_authdata);

			//if we could not store mail, retry with master
			if(rv){
				logprintf(log, LOG_ERROR, "Failed to store mail for %s (%d: %s), retrying one last time with master db\n", current_path->route.argument, rv, sqlite3_errmsg(database->conn));
				rv = mail_store_inbox(log, database->mail_storage.mailbox_master, mail, current_path);
				if(rv){
					logprintf(log, LOG_ERROR, "Failed to store mail: %s\n", sqlite3_errmsg(database->conn));
				}
			}
		}
	}
	else if(!strcmp(current_path->route.router, "redirect")){
		if(current_path->route.argument){
			//FIXME this should be ensured to be properly terminated
			strncpy(forward_path, current_path->route.argument, sizeof(forward_path) - 1);

			//mangle the envelope recipient according to the route
			//we're able to do this and not worry about exploitation from unsanitized input
			//because the path parser removes all comments from user-supplied paths
			strncpy(path_replacement, current_path->path, current_path->delimiter_position);
			path_replacement[current_path->delimiter_position] = 0;
			if(common_strrepl(forward_path, sizeof(forward_path), "(to-local)", path_replacement) < 0){
				logprintf(log, LOG_ERROR, "Failed to replace to-local variable in redirect router\n");
				//fail the transaction
				rv = -1;
			}

			if(!rv && current_path->path[current_path->delimiter_position] && common_strrepl(forward_path, sizeof(forward_path), "(to-domain)", current_path->path + current_path->delimiter_position + 1) < 0){
				logprintf(log, LOG_ERROR, "Failed to replace to-domain variable in redirect router\n");
				//fail the transaction
				rv = -1;
			}

			if(!rv){
				strncpy(path_replacement, mail->reverse_path.path, mail->reverse_path.delimiter_position);
				path_replacement[mail->reverse_path.delimiter_position] = 0;
				if(common_strrepl(forward_path, sizeof(forward_path), "(from-local)", path_replacement) < 0){
					logprintf(log, LOG_ERROR, "Failed to replace from-local variable in redirect router\n");
					//fail the transaction
					rv = -1;
				}
			}

			if(!rv && current_path->path[current_path->delimiter_position] && common_strrepl(forward_path, sizeof(forward_path), "(from-domain)", mail->reverse_path.path + mail->reverse_path.delimiter_position + 1) < 0){
				logprintf(log, LOG_ERROR, "Failed to replace from-domain variable in redirect router\n");
				//fail the transaction
				rv = -1;
			}

			//insert into outbound table
			if(!rv){
				rv = mail_store_outbox(log, database->mail_storage.outbox_master, NULL, forward_path, mail);
			}
		}
		else{
			logprintf(log, LOG_ERROR, "Redirect router without argument, failing transaction\n");
			//fail the transaction permanently
			rv = -1;
		}
	}
	else if(!strcmp(current_path->route.router, "handoff")){
		if(current_path->route.argument){
			//insert into outbound table
			rv = mail_store_outbox(log, database->mail_storage.outbox_master, current_path->route.argument, current_path->path, mail);
		}
		else{
			logprintf(log, LOG_ERROR, "Handoff router without argument, failing transaction\n");
			rv = -1;
		}
	}
	else if(!strcmp(current_path->route.router, "reject")){
		//this should probably never be reached as the path
		//of the user should already have been rejected.
		//otherwise, a mail with multiple recipients including
		//this one might get bounced on the basis of one
		//rejecting recipient
		rv = -1;
	}
	else if(!strcmp(current_path->route.router, "drop")){
		//this one is easy.
	}
	else{
		//TODO call plugins for other routers
	}

	if(rv>0){
		logprintf(log, LOG_INFO, "Additional information: %s\n", sqlite3_errmsg(database->conn));
	}

	return rv;
}
