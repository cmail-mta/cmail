int sqlite_delete_user(LOGGER log, sqlite3* db, const char* user) {

	char* sql = "DELETE FROM users WHERE user_name = ?";

	logprintf(log, LOG_INFO, "Username: %s\n", user);

	sqlite3_stmt* stmt = database_prepare(log, db, sql);

	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_INFO, "Cannot bind username.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 5;
	}

	if (sqlite3_changes(db) < 1) {
		printf("User not found.\n");
	}
	sqlite3_finalize(stmt);

	return 0;
}

int sqlite_add_user(LOGGER log, sqlite3* db, const char* user, const char* auth) {

        char* sql = "INSERT INTO users (user_name, user_authdata) VALUES (?, ?)";

        logprintf(log, LOG_INFO, "Username: %s\n", user);

        sqlite3_stmt* stmt = database_prepare(log, db, sql);
        if (!stmt) {
                return 2;
        }

        if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(log, LOG_ERROR, "Cannot bind username.\n");
                sqlite3_finalize(stmt);
                return 3;
        }

        if (auth) {
                if (sqlite3_bind_text(stmt, 2, auth, -1, SQLITE_STATIC) != SQLITE_OK) {
                        logprintf(log, LOG_ERROR, "Cannot bind authdata.");
                        sqlite3_finalize(stmt);
                        return 4;
                }
        } else {
                sqlite3_bind_null(stmt, 2);
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
                logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
		return 5;
        }
        sqlite3_finalize(stmt);

        return 0;
}
