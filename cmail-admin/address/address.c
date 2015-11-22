bool router_valid(char* router){
	if(!strcmp(router, "store") || !strcmp(router, "drop")){
		return true;
	}
	else if(!strcmp(router, "redirect") || !strcmp(router, "handoff")){
		return true;
	}
	else if(!strcmp(router, "reject")){
		return true;
	}
	return false;
}

int sqlite_get_address(LOGGER log, sqlite3* db, const char* expression) {
	char* sql = "SELECT address_expression, address_order, address_router, address_route FROM addresses WHERE address_expression LIKE ?";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, expression, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Cannot bind expression.\n");
		return 3;
	}

	int status;
	printf("%5s | %30s | %10s | %20s\n%s\n", "Order", "Expression", "Router", "Argument",
			"------+--------------------------------+------------+---------------------");

	while ((status = sqlite3_step(stmt)) == SQLITE_ROW) {
		printf("%5d | %30s | %10s | %20s\n", sqlite3_column_int(stmt, 1), (char*)sqlite3_column_text(stmt, 0), (char*)sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3) ? (char*)sqlite3_column_text(stmt, 3):"");
	}

	sqlite3_finalize(stmt);
	if (status == SQLITE_DONE) {
		return 0;
	}

	return status;
}

int sqlite_update_address(LOGGER log, sqlite3* db, long order, const char* router, const char* route) {
	char* sql = "UPDATE addresses SET address_router = ?, address_route = ? WHERE address_order = ?";
	int rv = 0;

	sqlite3_stmt* stmt = database_prepare(log, db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, router, -1, SQLITE_STATIC) != SQLITE_OK
			|| sqlite3_bind_int(stmt, 3, order) != SQLITE_OK
			|| sqlite3_bind_text(stmt, 2, route, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Failed to bind a statement parameters\n\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		rv = 5;
	}

	sqlite3_finalize(stmt);
	return rv;
}

int sqlite_delete_address(LOGGER log, sqlite3* db, long order) {
	char* sql = "DELETE FROM addresses WHERE address_order = ?;";
	int rv = 0;

	sqlite3_stmt* stmt = database_prepare(log, db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_int(stmt, 1, order) == SQLITE_OK) {
		if (sqlite3_step(stmt) != SQLITE_DONE) {
			logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
			rv = 5;
		}
	}
	else{
		logprintf(log, LOG_INFO, "Failed to bind a statement parameter\n\n");
		rv = 4;
	}


	if (!rv && sqlite3_changes(db) < 1) {
		printf("No such address, no modifications commited\n");
	}

	sqlite3_finalize(stmt);
	return 0;
}

int sqlite_add_address_order(LOGGER log, sqlite3* db, const char* expression, long order, const char* router, const char* route) {
	char* sql = "INSERT INTO addresses (address_expression, address_order, address_router, address_route) VALUES (?, ?, ?, ?)";
	int rv = 0;

	sqlite3_stmt* stmt = database_prepare(log, db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, expression, -1, SQLITE_STATIC) != SQLITE_OK
			|| sqlite3_bind_text(stmt, 3, router, -1, SQLITE_STATIC) != SQLITE_OK
			|| sqlite3_bind_text(stmt, 4, route, -1, SQLITE_STATIC) != SQLITE_OK
			|| sqlite3_bind_int(stmt, 2, order) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Failed to bind a statement parameter\n\n");
		sqlite3_finalize(stmt);
		return 4;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		rv = 5;
	}

	sqlite3_finalize(stmt);
	return rv;
}

int sqlite_add_address(LOGGER log, sqlite3* db, const char* expression, const char* router, const char* route) {
	char* sql = "INSERT INTO addresses (address_expression, address_router, address_route) VALUES (?, ?, ?)";
	int rv = 0;

	sqlite3_stmt* stmt = database_prepare(log, db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, expression, -1, SQLITE_STATIC) != SQLITE_OK
			|| sqlite3_bind_text(stmt, 2, router, -1, SQLITE_STATIC) != SQLITE_OK
			|| sqlite3_bind_text(stmt, 3, route, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Failed to bind a statement parameter\n\n");
		sqlite3_finalize(stmt);
		return 4;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		rv = 5;
	}

	sqlite3_finalize(stmt);
	return rv;
}

int sqlite_swap(LOGGER log, sqlite3* db, long first, long second) {
	int status = 0;
	char* expression = NULL;
	char* router = NULL;
	char* route = NULL;

	logprintf(log, LOG_DEBUG, "order1: %d, order2: %d\n", first, second);

	sqlite3_stmt* fetch = database_prepare(log, db, "SELECT address_expression, address_router, address_route FROM addresses WHERE address_order = ?;");
	sqlite3_stmt* update = database_prepare(log, db, "UPDATE addresses SET address_order = ? WHERE address_order = ?;");
	if(!fetch || !update || sqlite3_bind_int(fetch, 1, first)){
		logprintf(log, LOG_ERROR, "Failed to prepare or bind statement\n");
		return -1;
	}

	switch(sqlite3_step(fetch)){
		case SQLITE_ROW:
			expression = common_strdup((char*)sqlite3_column_text(fetch, 0));
			router = common_strdup((char*)sqlite3_column_text(fetch, 1));
			if(sqlite3_column_text(fetch, 2)){
				route = common_strdup((char*)sqlite3_column_text(fetch, 2));
			}
			
			if(!expression || !router){
				status = 19;
			}
			break;
		case SQLITE_DONE:
			logprintf(log, LOG_ERROR, "No expression with order %d\n", first);
			status = 20;
			break;
		default:
			logprintf(log, LOG_ERROR, "Unhandled failure: %s\n", sqlite3_errmsg(db));
			break;
	}

	sqlite3_finalize(fetch);

	if(!status){
		sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);

		if (sqlite_delete_address(log, db, first) != 0) {
			status = 21;
		}

		if (!status && (sqlite3_bind_int(update, 1, first) != SQLITE_OK || sqlite3_bind_int(update, 2, second) != SQLITE_OK)) {
			status = 22;
		}

		switch(sqlite3_step(update)){
			case SQLITE_DONE:
			case SQLITE_OK:
				break;
			default:
				logprintf(log, LOG_ERROR, "Failed to update address record: %s\n", sqlite3_errmsg(db));
				status = 23;
				break;
		}

		if (!status && sqlite_add_address_order(log, db, expression, second, router, route) != 0) {
			status = 24;
		}

		if (status) {
			sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
		} else {
			sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
		}
	}

	sqlite3_finalize(update);

	if(expression){
		free(expression);
	}

	if(router){
		free(router);
	}

	if(route){
		free(route);
	}

	return status;
}
