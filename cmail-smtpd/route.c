MAILROUTE route_query(LOGGER log, DATABASE* database, bool route_inbound, char* user){
	MAILROUTE route = {
		.router = NULL,
		.argument = NULL
	};
	sqlite3_stmt* stmt=((route_inbound)?database->query_inrouter:database->query_outrouter);
	int status;

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
				route.router = calloc(sqlite3_column_bytes(stmt, 0) + 1, sizeof(char));
				if(route.router){
					strncpy(route.router, (char*)sqlite3_column_text(stmt, 0), sqlite3_column_bytes(stmt, 0) + 1);
				}
				else{
					logprintf(log, LOG_ERROR, "Failed to allocate memory for ROUTE structure\n");
				}
			}

			if(sqlite3_column_bytes(stmt, 1)){
				route.argument = calloc(sqlite3_column_bytes(stmt, 1) + 1, sizeof(char));
				if(route.argument){
					strncpy(route.argument, (char*)sqlite3_column_text(stmt, 1), sqlite3_column_bytes(stmt, 1) + 1);
				}
				else{
					logprintf(log, LOG_ERROR, "Failed to allocate memory for ROUTE structure\n");
				}

			}
			break;
		case SQLITE_DONE:
			logprintf(log, LOG_ERROR, "Database contains no router definition for %s\n", user);
			break;
		default:
			logprintf(log, LOG_WARNING, "Unhandled query return value: %s\n", sqlite3_errmsg(database->conn));
			break;
	}

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

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

int route_inbound(LOGGER log, DATABASE* database, MAIL* mail, MAILPATH* current_path){
	int rv = 0;
	USER_DATABASE* user_db;
	char path_replacement[SMTP_MAX_PATH_LENGTH];
	char forward_path[SMTP_MAX_PATH_LENGTH];
	MAILROUTE route = route_query(log, database, true, current_path->resolved_user);

	//reject the path if the user does not have a router table entry
	if(!route.router){
		return -1;
	}

	logprintf(log, LOG_DEBUG, "Inbound router %s (%s) for %s\n", route.router, route.argument ? route.argument:"none", current_path->path);

	if(route.router){
		if(!strcmp(route.router, "store")){
			//insert into (user) mail table
			if(!route.argument){
				//the simple case, we insert into the master db
				rv = mail_store_inbox(log, database->mail_storage.mailbox_master, mail, current_path);
			}
			else{
				//this works regardless of the changes from schema update 2
				//because users.user_database is aliased into route.argument by the query

				//get user storage database entry
				user_db = database_userdb(log, database, route.argument);
				if(!user_db){
					//try to refresh the user database set
					database_refresh(log, database);
					user_db = database_userdb(log, database, route.argument);

					if(!user_db){
						//as last resort, store to master db
						logprintf(log, LOG_WARNING, "Stored mail for user %s to master instead of defined database\n", current_path->resolved_user);
						rv = mail_store_inbox(log, database->mail_storage.mailbox_master, mail, current_path);
					}
					else{
						rv = mail_store_inbox(log, user_db->mailbox, mail, current_path);
					}
				}
				else{
					rv = mail_store_inbox(log, user_db->mailbox, mail, current_path);
				}
			}

			//if we could not store mail, retry with master
			if(rv){
				logprintf(log, LOG_WARNING, "Failed to store mail for %s (%d: %s), retrying one last time with master db\n", current_path->resolved_user, rv, sqlite3_errmsg(database->conn));
				rv = mail_store_inbox(log, database->mail_storage.mailbox_master, mail, current_path);
			}
		}
		else if(!strcmp(route.router, "forward")){
			if(route.argument){
				strncpy(forward_path, route.argument, sizeof(forward_path) - 1);

				//mangle the envelope recipient according to the route
				//we're able to do this and not worry about exploitation from unsanitized input
				//because the path parser removes all comments from user-supplied paths
				strncpy(path_replacement, current_path->path, current_path->delimiter_position);
				path_replacement[current_path->delimiter_position] = 0;
				if(common_strrepl(forward_path, sizeof(forward_path), "(to-local)", path_replacement) < 0){
					logprintf(log, LOG_ERROR, "Failed to replace to-local variable in forward router\n");
					//fail the transaction
					rv = -1;
				}

				if(!rv && current_path->path[current_path->delimiter_position] && common_strrepl(forward_path, sizeof(forward_path), "(to-domain)", current_path->path + current_path->delimiter_position + 1) < 0){
					logprintf(log, LOG_ERROR, "Failed to replace to-domain variable in forward router\n");
					//fail the transaction
					rv = -1;
				}

				if(!rv){
					strncpy(path_replacement, mail->reverse_path.path, mail->reverse_path.delimiter_position);
					path_replacement[mail->reverse_path.delimiter_position] = 0;
					if(common_strrepl(forward_path, sizeof(forward_path), "(from-local)", path_replacement) < 0){
						logprintf(log, LOG_ERROR, "Failed to replace from-local variable in forward router\n");
						//fail the transaction
						rv = -1;
					}
				}

				if(!rv && current_path->path[current_path->delimiter_position] && common_strrepl(forward_path, sizeof(forward_path), "(from-domain)", mail->reverse_path.path + mail->reverse_path.delimiter_position + 1) < 0){
					logprintf(log, LOG_ERROR, "Failed to replace from-domain variable in forward router\n");
					//fail the transaction
					rv = -1;
				}

				//insert into outbound table
				if(!rv){
					rv = mail_store_outbox(log, database->mail_storage.outbox_master, NULL, forward_path, mail);
				}
			}
		}
		else if(!strcmp(route.router, "handoff")){
			if(route.argument){
				//insert into outbound table
				rv = mail_store_outbox(log, database->mail_storage.outbox_master, route.argument, current_path->path, mail);
			}
		}
		else if(!strcmp(route.router, "reject")){
			//this should probably never be reached as the path
			//of the user should already have been rejected.
			//otherwise, a mail with multiple recipients including
			//this one might get bounced on the basis of one
			//rejecting recipient
			rv = -1;
		}
		else if(!strcmp(route.router, "drop")){
			//this one is easy.
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

int route_outbound(LOGGER log, DATABASE* database, char* user, MAILPATH* reverse_path){
	int rv = 0;
	MAILROUTE route = route_query(log, database, false, user);

	//fail any path if the authenticated user has no router table entry
	if(!route.router){
		return -1;
	}

	logprintf(log, LOG_DEBUG, "Outbound router %s (%s)\n", route.router, route.argument ? route.argument:"none");

	if(!strcmp(route.router, "reject")){
		rv = -1;
	}
	else if(!strcmp(route.router, "defined")){
		if(!reverse_path->resolved_user || strcmp(user, reverse_path->resolved_user)){
			logprintf(log, LOG_INFO, "Reverse path %s does not belong to user %s\n", reverse_path->path, user);
			rv = -1;
		}
		//else, ok
	}
	else if(!strcmp(route.router, "any")){
		//ok.
	}

	route_free(&route);
	return rv;
}
