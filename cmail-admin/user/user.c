int sqlite_get(LOGGER log, sqlite3* db, const char* username) {


	char* sql = "SELECT user_name, user_authdata IS NOT NULL AS has_login FROM users WHERE user_name LIKE ?";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);

	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Cannot bind username.\n");
		return 3;
	}

	int status;
	const char* user;
	int login;
	while ((status = sqlite3_step(stmt)) == SQLITE_ROW) {
		
		user = (const char*) sqlite3_column_text(stmt, 0);
		login = sqlite3_column_int(stmt, 1);
		printf("%s | %d\n", user, login);
	}

	sqlite3_finalize(stmt);
	if (status == SQLITE_DONE) {
		return 0;
	}

	return status;
}

int sqlite_get_all(LOGGER log, sqlite3* db) {

	char* sql = "SELECT user_name, user_authdata IS NOT NULL AS has_login FROM users";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);

	if (!stmt) {
		return 2;
	}

	int status;
	const char* username;
	int login;
	while ((status = sqlite3_step(stmt)) == SQLITE_ROW) {

		username = (const char*) sqlite3_column_text(stmt, 0);
		login = sqlite3_column_int(stmt, 1);

		printf("%s | %d\n", username, login);
	}

	sqlite3_finalize(stmt);

	if (status == SQLITE_DONE) {
		return 0;
	}
	return status;
}

int sqlite_update_user(LOGGER log, sqlite3* db, const char* user, const char* auth, char* sql) {

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

int sqlite_set_password(LOGGER log, sqlite3* db, const char* user, const char* auth) {

	char* sql = "UPDATE users SET user_authdata = ?2 WHERE user_name = ?1";

	return sqlite_update_user(log, db, user, auth, sql);
}


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

	return sqlite_update_user(log, db, user, auth, sql);
}
