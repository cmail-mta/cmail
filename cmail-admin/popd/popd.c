int sqlite_get_popd(sqlite3* db, const char* username) {


	char* sql = "SELECT pop_user, pop_lock FROM popd WHERE pop_user LIKE ?";

	sqlite3_stmt* stmt = database_prepare(db, sql);

	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_ERROR, "Cannot bind expression.\n");
		return 3;
	}

	int status;
	const char* user;
	int lock;

	printf("%20s | %5s\n%s\n", "User", "Lock", "---------------------+------");

	while ((status = sqlite3_step(stmt)) == SQLITE_ROW) {

		user = (const char*) sqlite3_column_text(stmt, 0);
		lock = sqlite3_column_int(stmt, 1);
		printf("%20s | %5d\n", user, lock);
	}

	sqlite3_finalize(stmt);
	if (status == SQLITE_DONE) {
		return 0;
	}

	return status;
}

int sqlite_get_all_popd(sqlite3* db) {

	char* sql = "SELECT pop_user, pop_lock FROM popd";
	sqlite3_stmt* stmt = database_prepare(db, sql);

	if (!stmt) {
		return 2;
	}
	int status;
	const char* user;
	int lock;

	while ((status = sqlite3_step(stmt)) == SQLITE_ROW) {

		user = (const char*) sqlite3_column_text(stmt, 0);
		lock = sqlite3_column_int(stmt, 1);
		printf("%s | %d\n", user, lock);
	}


	sqlite3_finalize(stmt);

	if (status == SQLITE_DONE) {
		return 0;
	}
	return status;
}

int sqlite_add_popd(sqlite3* db, const char* user) {

	char* sql = "INSERT INTO popd (pop_user) VALUES (?)";

        sqlite3_stmt* stmt = database_prepare(db, sql);
        if (!stmt) {
                return 2;
        }

	if (sqlite3_bind_text(stmt, 1, user , -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(LOG_ERROR, "Cannot bind user.\n");
                sqlite3_finalize(stmt);
                return 91;
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
                logprintf(LOG_ERROR, "%s\n", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
		return 5;
        }
        sqlite3_finalize(stmt);
        return 0;
}

int sqlite_update_popd(sqlite3* db, const char* user, int lock) {

	char* sql = "UPDATE popd SET pop_lock = ? WHERE pop_user = ?";

        sqlite3_stmt* stmt = database_prepare(db, sql);
        if (!stmt) {
                return 2;
        }

        if (sqlite3_bind_int(stmt, 1, lock) != SQLITE_OK) {
		logprintf(LOG_ERROR, "Cannot bind popd lock.\n");
		return 81;
	}

	if (sqlite3_bind_text(stmt, 2, user , -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(LOG_ERROR, "Cannot bind user.\n");
                sqlite3_finalize(stmt);
                return 82;
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
                logprintf(LOG_ERROR, "%s\n", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
		return 5;
        }
        sqlite3_finalize(stmt);
	if (sqlite3_changes(db) < 1) {
		printf("POPD entry not found.\n");
	}
        return 0;
}

int sqlite_delete_popd(sqlite3* db, const char* user) {

	char* sql = "DELETE FROM popd WHERE pop_user = ?";

	sqlite3_stmt* stmt = database_prepare(db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_INFO, "Cannot bind user.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 5;
	}

	if (sqlite3_changes(db) < 1) {
		printf("POPD entry not found.\n");
	}
	sqlite3_finalize(stmt);

	return 0;
}
