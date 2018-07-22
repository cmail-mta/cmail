int sqlite_get_msa(sqlite3* db, const char* username) {
	char* sql = "SELECT smtpd_user, smtpd_router, smtpd_route FROM smtpd WHERE smtpd_user LIKE ?";

	sqlite3_stmt* stmt = database_prepare(db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_ERROR, "Cannot bind expression.\n");
		return 3;
	}

	int status;
	printf("%20s | %10s | %15s\n%s\n", "User", "Router", "Argument",
			"---------------------+------------+-----------------");

	while ((status = sqlite3_step(stmt)) == SQLITE_ROW) {
		printf("%20s | %10s | %15s\n", (char*)sqlite3_column_text(stmt, 0), (char*)sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2) ? (char*)sqlite3_column_text(stmt, 2):"");
	}

	sqlite3_finalize(stmt);
	return (status == SQLITE_DONE) ? 0:status;
}

int sqlite_update_msa(sqlite3* db, const char* user, const char* router, const char* argument) {
	char* sql = "UPDATE smtpd SET smtpd_router = ?, smtpd_route = ? WHERE smtpd_user = ?;";

	sqlite3_stmt* stmt = database_prepare(db, sql);
	if (!stmt) {
		return 2;
	}

	int rv = 0;

	if (sqlite3_bind_text(stmt, 1, router, -1, SQLITE_STATIC) != SQLITE_OK
			|| sqlite3_bind_text(stmt, 2, argument, -1, SQLITE_STATIC) != SQLITE_OK
			|| sqlite3_bind_text(stmt, 3, user, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_ERROR, "Failed to bind a statement parameter\n\n");
		sqlite3_finalize(stmt);
		return 72;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		rv = 5;
	}

	sqlite3_finalize(stmt);
	
	if (sqlite3_changes(db) < 1) {
		printf("No entry found, no modifications performed\n");
	}
	
	return rv;
}

int sqlite_delete_msa(sqlite3* db, const char* user) {
	char* sql = "DELETE FROM smtpd WHERE smtpd_user = ?;";

	sqlite3_stmt* stmt = database_prepare(db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_INFO, "Failed to bind a statement parameter\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 5;
	}

	if (sqlite3_changes(db) < 1) {
		printf("No entry found, none modified\n");
	}

	sqlite3_finalize(stmt);
	return 0;
}

int sqlite_add_msa(sqlite3* db, const char* user, const char* router, const char* argument) {
	char* sql = router ? "INSERT INTO smtpd (smtpd_user, smtpd_router, smtpd_route) VALUES (?, ?, ?);":"INSERT INTO smtpd (smtpd_user) VALUES (?);";
	logprintf(LOG_DEBUG, "Using statement %s\n", sql);

	sqlite3_stmt* stmt = database_prepare(db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK
			|| (router && sqlite3_bind_text(stmt, 2, router, -1, SQLITE_STATIC) != SQLITE_OK)
			|| (router && sqlite3_bind_text(stmt, 3, argument, -1, SQLITE_STATIC) != SQLITE_OK)) {
		logprintf(LOG_ERROR, "Failed to bind a statement parameter\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 5;
	}
	sqlite3_finalize(stmt);

	return 0;
}
