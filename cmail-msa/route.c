MAILROUTE route_query(LOGGER log, DATABASE* database, bool route_inbound, char* user){
	MAILROUTE route = {
		.router = NULL,
		.argument = NULL
	};
	sqlite3_stmt* stmt=((route_inbound)?database->query_inrouter:database->query_outrouter);
	int status;
	char* recursion_temp;

	if(!user){
		return route;
	}

	status=sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC);
	if(status!=SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind router user parameter: %s\n", sqlite3_errmsg(database->conn));
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		return route;
	}

	switch(sqlite3_step(stmt)){
		case SQLITE_ROW:
			//copy data
			if(sqlite3_column_bytes(stmt, 0)){
				route.router=calloc(sqlite3_column_bytes(stmt, 0)+1, sizeof(char));
				if(route.router){
					strncpy(route.router, (char*)sqlite3_column_text(stmt, 0), sqlite3_column_bytes(stmt, 0)+1);
				}
				else{
					logprintf(log, LOG_ERROR, "Failed to allocate memory for ROUTE structure\n");
				}
			}

			if(sqlite3_column_bytes(stmt, 1)){
				route.argument=calloc(sqlite3_column_bytes(stmt, 1)+1, sizeof(char));
				if(route.argument){
					strncpy(route.argument, (char*)sqlite3_column_text(stmt, 1), sqlite3_column_bytes(stmt, 1)+1);
				}
				else{
					logprintf(log, LOG_ERROR, "Failed to allocate memory for ROUTE structure\n");
				}

			}
			break;
		case SQLITE_DONE:
			logprintf(log, LOG_ERROR, "User to be routed to (%s) does not exist\n", user);
			break;
		default:
			logprintf(log, LOG_WARNING, "Unhandled query return value: %s\n", sqlite3_errmsg(database->conn));
			break;
	}

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	//handle alias via recursion
	if(route.router && !strcmp(route.router, "alias")){
		free(route.router);
		recursion_temp=route.argument;
		logprintf(log, LOG_INFO, "Recursing for user %s alias %s\n", user, recursion_temp);
		route=route_query(log, database, route_inbound, recursion_temp);
		free(recursion_temp);
	}

	return route;
}

void route_free(MAILROUTE* route){
	if(route){
		if(route->router){
			free(route->router);
			route->router=NULL;
		}

		if(route->argument){
			free(route->argument);
			route->argument=NULL;
		}
	}
}

int route_inbound(LOGGER log, DATABASE* database, MAIL* mail, MAILPATH* current_path){
	int rv=0;
	USER_DATABASE* user_db;
	MAILROUTE route=route_query(log, database, true, current_path->resolved_user);

	logprintf(log, LOG_DEBUG, "Inbound router %s (%s) for %s\n", route.router, route.argument?route.argument:"none", current_path->path);

	if(route.router){
		if(!strcmp(route.router, "store")){
			//insert into (user) mail table
			if(!route.argument){
				//the simple case, we insert into the master db
				rv=mail_store_inbox(log, database->mail_storage.mailbox_master, mail, current_path);
			}
			else{
				//get user storage database entry
				user_db=database_userdb(log, database, route.argument);
				if(!user_db){
					//try to refresh the user database set
					database_refresh(log, database);
					user_db=database_userdb(log, database, route.argument);
				
					if(!user_db){
						//as last resort, store to master db	
						logprintf(log, LOG_WARNING, "Stored mail for user %s to master instead of defined database\n", current_path->resolved_user);
						rv=mail_store_inbox(log, database->mail_storage.mailbox_master, mail, current_path);
					}
					else{
						rv=mail_store_inbox(log, user_db->mailbox, mail, current_path);
					}
				}
				else{
					rv=mail_store_inbox(log, user_db->mailbox, mail, current_path);
				}
			}

			//if we could not store mail, retry with master
			if(rv){
				logprintf(log, LOG_WARNING, "Failed to store mail for %s, retrying one last time with master db\n", current_path->resolved_user);
				rv=mail_store_inbox(log, database->mail_storage.mailbox_master, mail, current_path);
			}
		}
		else if(!strcmp(route.router, "forward")){
			//insert into outbound table
			rv=mail_store_outbox(log, database->mail_storage.outbox_master, NULL, route.argument, mail);
		}
		else if(!strcmp(route.router, "handoff")){
			//insert into outbound table
			rv=mail_store_outbox(log, database->mail_storage.outbox_master, route.argument, current_path->path, mail);
		}
		else if(!strcmp(route.router, "reject")){
			//this should probably never be reached as the path
			//of the user should already have been rejected.
			//otherwise, a mail with multiple recipients including
			//this one might get bounced on the basis of one
			//rejecting recipient
			rv=-1;
		}
		else{
			//TODO call plugins for other routers
		}
		
		if(rv>0){
			logprintf(log, LOG_INFO, "Additional information: %s\n", sqlite3_errmsg(database->conn));
		}
	}
	
	route_free(&route);
	return rv;
}

int route_outbound(LOGGER log, DATABASE* database, MAIL* mail, MAILPATH* current_path){
	MAILROUTE route=route_query(log, database, false, NULL); //TODO implement this properly

	logprintf(log, LOG_DEBUG, "Outbound router %s (%s)\n", route.router, route.argument?route.argument:"none");
	//TODO implement outbound routers
	
	logprintf(log, LOG_WARNING, "NOT YET IMPLEMENTED: OUTBOUND ROUTING\n");

	route_free(&route);
	return -1;
}
