MAILROUTE route_query(LOGGER log, DATABASE database, bool route_inbound, char* user){
	MAILROUTE route = {
		.router = NULL,
		.argument = NULL
	};
	sqlite3_stmt* stmt=((route_inbound)?database.query_inrouter:database.query_outrouter);
	int status;
	char* recursion_temp;

	status=sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC);
	if(status!=SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind router user parameter: %s\n", sqlite3_errmsg(database.conn));
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
			logprintf(log, LOG_WARNING, "Unhandled query return value: %s\n", sqlite3_errmsg(database.conn));
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

int route_apply_inbound(LOGGER log, DATABASE database, char* user, MAIL* mail){
	int rv=0;
	MAILROUTE route=route_query(log, database, true, user);

	logprintf(log, LOG_DEBUG, "Inbound router %s (%s)\n", route.router, route.argument);

	if(route.router){
		if(!strcmp(route.router, "store")){
			//TODO insert into mail table
		}
		else if(!strcmp(route.router, "forward")){
			//TODO insert into outbound table
		}
		else if(!strcmp(route.router, "handoff")){
			//TODO insert into outbound table
		}
		else if(!strcmp(route.router, "reject")){
			//this should probably never be reached as the path
			//of the user should already have been rejected.
			//otherwise, a mail with multiple recipients including
			//this one might get bounced on the basis of one
			//rejecting recipient
			rv=-1;
		}
	}
	
	route_free(&route);
	return rv;
}

int route_apply_outbound(LOGGER log, DATABASE database, char* user, MAIL* mail){
	MAILROUTE route=route_query(log, database, false, user);

	logprintf(log, LOG_DEBUG, "Outbound router %s (%s)\n", route.router, route.argument);
	
	route_free(&route);
	return -1;
}