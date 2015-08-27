sqlite3_stmt* sqlite_get_address_raw(LOGGER log, sqlite3* db, const char* expression) {
	char* sql = "SELECT address_user, address_order FROM addresses WHERE address_expression = ?";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);

	if (!stmt) {
		return NULL;
	}

	if (sqlite3_bind_text(stmt, 1, expression, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Cannot bind expression.\n");
		return NULL;
	}

	if (sqlite3_step(stmt) != SQLITE_ROW) {
		logprintf(log, LOG_ERROR, "No row found.");
		sqlite3_finalize(stmt);
		return NULL;
	}

	return stmt;

}

int sqlite_get_address(LOGGER log, sqlite3* db, const char* expression) {


	char* sql = "SELECT address_expression, address_user, address_order FROM addresses WHERE address_expression LIKE ?";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);

	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, expression, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Cannot bind expression.\n");
		return 3;
	}

	int status;
	const char* user;
	const char* address;
	int order;

	printf("%5s | %20s | %30s\n%s\n", "Order", "User", "address",
			"------+----------------------+-------------------------------");

	while ((status = sqlite3_step(stmt)) == SQLITE_ROW) {

		address = (const char*) sqlite3_column_text(stmt, 0);
		user = (const char*) sqlite3_column_text(stmt, 1);
		order = sqlite3_column_int(stmt, 2);
		printf("%5d | %20s | %30s\n", order, user, address);
	}

	sqlite3_finalize(stmt);
	if (status == SQLITE_DONE) {
		return 0;
	}

	return status;
}

int sqlite_get_all_addresses(LOGGER log, sqlite3* db) {

	char* sql = "SELECT address_expression, address_user, address_order FROM addresses";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);

	if (!stmt) {
		return 2;
	}

	int status;
	const char* username;
	const char* address;
	int order;
	while ((status = sqlite3_step(stmt)) == SQLITE_ROW) {

		address = (const char*) sqlite3_column_text(stmt, 0);
		username = (const char*) sqlite3_column_text(stmt, 1);
		order = sqlite3_column_int(stmt, 2);

		printf("%d | %s | %s\n", order, address, username);
	}

	sqlite3_finalize(stmt);

	if (status == SQLITE_DONE) {
		return 0;
	}
	return status;
}

int sqlite_update_address(LOGGER log, sqlite3* db, const char* expression, const char* user, long order) {

	char* sql = "UPDATE addresses SET address_order = ?, address_user = ? WHERE address_expression = ?";

	logprintf(log, LOG_INFO, "expression: %s, user: %s, order: %d\n", expression, user, order);

        sqlite3_stmt* stmt = database_prepare(log, db, sql);
        if (!stmt) {
                return 2;
        }

        if (sqlite3_bind_text(stmt, 2, user, -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(log, LOG_ERROR, "Cannot bind username.\n");
                sqlite3_finalize(stmt);
                return 3;
        }

	if (sqlite3_bind_int(stmt, 1, order) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Cannot bind order\n");
		sqlite3_finalize(stmt);
		return 3;
	}

       	if (sqlite3_bind_text(stmt, 3, expression, -1, SQLITE_STATIC) != SQLITE_OK) {
       		logprintf(log, LOG_ERROR, "Cannot bind expression.");
                sqlite3_finalize(stmt);
        	return 4;
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
                logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
		return 5;
        }
        sqlite3_finalize(stmt);

        return 0;
}

int sqlite_delete_address(LOGGER log, sqlite3* db, const char* expression) {

	char* sql = "DELETE FROM addresses WHERE address_expression = ?";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, expression, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_INFO, "Cannot bind address.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 5;
	}

	if (sqlite3_changes(db) < 1) {
		printf("Address not found.\n");
	}
	sqlite3_finalize(stmt);

	return 0;
}

int sqlite_add_address_order(LOGGER log, sqlite3* db, const char* expression, const char* user, long order) {

        char* sql = "INSERT INTO addresses (address_expression, address_user, address_order) VALUES (?, ?, ?)";

        sqlite3_stmt* stmt = database_prepare(log, db, sql);
        if (!stmt) {
                return 2;
        }

       	if (sqlite3_bind_text(stmt, 1, expression, -1, SQLITE_STATIC) != SQLITE_OK) {
       		logprintf(log, LOG_ERROR, "Cannot bind expression.");
                sqlite3_finalize(stmt);
        	return 4;
        }

        if (sqlite3_bind_text(stmt, 2, user, -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(log, LOG_ERROR, "Cannot bind username.\n");
                sqlite3_finalize(stmt);
                return 3;
        }

	if (sqlite3_bind_int(stmt, 3, order) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Cannot bind order\n");
		sqlite3_finalize(stmt);
		return 3;
	}

        if (sqlite3_step(stmt) != SQLITE_DONE) {
                logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
		return 5;
        }
        sqlite3_finalize(stmt);

        return 0;
}

int sqlite_add_address(LOGGER log, sqlite3* db, const char* expression, const char* user) {

        char* sql = "INSERT INTO addresses (address_expression, address_user) VALUES (?, ?)";

        sqlite3_stmt* stmt = database_prepare(log, db, sql);
        if (!stmt) {
                return 2;
        }

       	if (sqlite3_bind_text(stmt, 1, expression, -1, SQLITE_STATIC) != SQLITE_OK) {
       		logprintf(log, LOG_ERROR, "Cannot bind expression.");
                sqlite3_finalize(stmt);
        	return 4;
        }

        if (sqlite3_bind_text(stmt, 2, user, -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(log, LOG_ERROR, "Cannot bind username.\n");
                sqlite3_finalize(stmt);
                return 3;
        }


        if (sqlite3_step(stmt) != SQLITE_DONE) {
                logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
		return 5;
        }
        sqlite3_finalize(stmt);

        return 0;
}

int sqlite_switch(LOGGER log, sqlite3* db, const char* expression1, const char* expression2) {

	sqlite3_stmt* stmt1 = sqlite_get_address_raw(log, db, expression1);
	sqlite3_stmt* stmt2 = sqlite_get_address_raw(log, db, expression2);

	if (!stmt1 || !stmt2) {
		return 20;
	}

	int order1 = sqlite3_column_int(stmt1, 1);
	const char* user1 = (const char*) sqlite3_column_text(stmt1, 0);
	const char* user2 = (const char*) sqlite3_column_text(stmt2, 0);
	int order2 = sqlite3_column_int(stmt2, 1);


	sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);

	if (sqlite_delete_address(log, db, expression1) != 0) {
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
		sqlite3_finalize(stmt1);
		sqlite3_finalize(stmt2);
		return 21;
	}

	logprintf(log, LOG_DEBUG, "order1: %d, order2: %d\n", order1, order2);

	if (sqlite_update_address(log, db, expression2, user2, order1) != 0) {
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
		sqlite3_finalize(stmt1);
		sqlite3_finalize(stmt2);
		return 22;
	}

	if (sqlite_add_address_order(log, db, expression1, user1, order2) != 0) {
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
		sqlite3_finalize(stmt1);
		sqlite3_finalize(stmt2);
		return 22;
	}

	sqlite3_finalize(stmt1);
	sqlite3_finalize(stmt2);
	sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);

	return 0;
}
