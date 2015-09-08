int sqlite_get(LOGGER log, sqlite3* db, const char* username) {
	char* sql = "SELECT user_name, user_authdata IS NOT NULL AS has_login, user_alias, user_database, link_count FROM users LEFT JOIN (SELECT address_route, count(address_route) AS link_count FROM addresses WHERE address_router = 'store' GROUP BY address_route) ON (address_route = user_name) WHERE user_name LIKE ?";
	int status;

	sqlite3_stmt* stmt = database_prepare(log, db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Cannot bind username.\n");
		return 3;
	}

	printf("%20s | %5s | %15s | %16s | %14s\n%s\n", "User", "Auth", "Alias", "Linked addresses", "User database",
			"---------------------+-------+-----------------+------------------+----------------");

	while ((status = sqlite3_step(stmt)) == SQLITE_ROW) {
		printf("%20s | %5d | %15s | %16d | %20s\n", (char*)sqlite3_column_text(stmt, 0), 
				sqlite3_column_int(stmt, 1), 
				(sqlite3_column_text(stmt, 2)) ? (char*) sqlite3_column_text(stmt, 2) : "", 
				sqlite3_column_int(stmt, 4),
				(sqlite3_column_text(stmt, 3)) ? (char*) sqlite3_column_text(stmt, 3) : "");
	}

	sqlite3_finalize(stmt);
	return (status == SQLITE_DONE) ? 0:-1;
}

int sqlite_update_user(LOGGER log, sqlite3* db, const char* user, const char* data, char* sql) {
	sqlite3_stmt* stmt = database_prepare(log, db, sql);
	int status = 0;

	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK
			|| sqlite3_bind_text(stmt, 2, data, -1, SQLITE_STATIC) != SQLITE_OK) {
			logprintf(log, LOG_ERROR, "Failed to bind a statement parameter\n");
		status = 4;
	}

	if (!status && sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(log, LOG_ERROR, "Failed to update: %s\n", sqlite3_errmsg(db));
		status = 5;
	}

	if (!status && sqlite3_changes(db) < 1) {
		printf("No such user, no changes made\n");
	}

	sqlite3_finalize(stmt);
	return status;
}

int sqlite_set_alias(LOGGER log, sqlite3* db, const char* user, const char* alias){
	char* sql = "UPDATE users SET user_alias = ?2 WHERE user_name = ?1;";
	return sqlite_update_user(log, db, user, alias, sql);
}

int sqlite_set_userdb(LOGGER log, sqlite3* db, const char* user, const char* userdb){
	char* sql = "UPDATE users SET user_database = ?2 WHERE user_name = ?1;";
	return sqlite_update_user(log, db, user, userdb, sql);
}

int sqlite_set_password(LOGGER log, sqlite3* db, const char* user, const char* auth) {
	char* sql = "UPDATE users SET user_authdata = ?2 WHERE user_name = ?1;";
	return sqlite_update_user(log, db, user, auth, sql);
}

int sqlite_delete_user(LOGGER log, sqlite3* db, const char* user) {
	char* sql = "DELETE FROM users WHERE user_name = ?";
	int status = 0;

	sqlite3_stmt* stmt = database_prepare(log, db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_INFO, "Cannot bind username.\n");
		status = 3;
	}

	if (!status && sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		status = 5;
	}

	if (!status && sqlite3_changes(db) < 1) {
		printf("No such user, no changes made\n");
	}

	sqlite3_finalize(stmt);
	return status;
}

int sqlite_add_user(LOGGER log, sqlite3* db, const char* user, const char* auth) {
	char* sql = "INSERT INTO users (user_name, user_authdata) VALUES (?, ?)";
	return sqlite_update_user(log, db, user, auth, sql);
}
