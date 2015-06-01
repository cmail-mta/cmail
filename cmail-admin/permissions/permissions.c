int sqlite_exec_select(LOGGER log, sqlite3_stmt* stmt) {

	int status;
	const char* user;
	const char* right;

	while ((status = sqlite3_step(stmt)) == SQLITE_ROW) {

		user = (const char*) sqlite3_column_text(stmt, 0);
		right = (const char*) sqlite3_column_text(stmt, 1);
		printf("%s | %s\n", user, right);
	}

	sqlite3_finalize(stmt);
	if (status == SQLITE_DONE) {
		return 0;
	}

	return status;
}

int sqlite_get_delegated_users(LOGGER log, sqlite3* db, const char* username) {

	char* sql = "SELECT api_user, api_delegate FROM api_user_delegates WHERE api_user LIKE ?";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);

	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Cannot bind expression.\n");
		return 3;
	}

	return sqlite_exec_select(log, stmt);

}

int sqlite_get_delegated_addresses(LOGGER log, sqlite3* db, const char* username) {

	char* sql = "SELECT api_user, api_expression FROM api_address_delegates WHERE api_user LIKE ?";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);

	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Cannot bind expression.\n");
		return 3;
	}

	return sqlite_exec_select(log, stmt);
}

int sqlite_get_delegated(LOGGER log, sqlite3* db, const char* username) {

	printf("Delegated users:\n");
	int status = sqlite_get_delegated_users(log, db, username);

	if (status > 0) {
		return status;
	}

	printf("\nDelegated address space:\n");
	return sqlite_get_delegated_addresses(log, db, username);
}

int sqlite_get_delegated_all(LOGGER log, sqlite3* db) {

	char* sql_user = "SELECT api_user, api_delegate FROM api_user_delegates";
	char* sql_address = "SELECT api_user, api_expression FROM api_address_delegates";
	int status;

	sqlite3_stmt* stmt_user = database_prepare(log, db, sql_user);
	if (!stmt_user) {
		return 2;
	}

	printf("Delegated users:\n");
	status = sqlite_exec_select(log, stmt_user);

	if (status > 0) {
		return status;
	}

	sqlite3_stmt* stmt_address = database_prepare(log, db, sql_address);

	if (!stmt_address) {
		return 2;
	}

	printf("\nDelegated address space:\n");

	return sqlite_exec_select(log, stmt_address);

}

int sqlite_get_rights(LOGGER log, sqlite3* db, const char* username) {


	char* sql = "SELECT api_user, api_right FROM api_access WHERE api_user LIKE ?";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);

	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Cannot bind expression.\n");
		return 3;
	}

	return sqlite_exec_select(log, stmt);
}

int sqlite_get_rights_by_right(LOGGER log, sqlite3* db, const char* right) {

	char*  sql = "SELECT api_user, api_right FROM api_access WHERE api_right LIKE ?";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);

	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, right, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Cannot bind right.\n");
		return 3;
	}

	return sqlite_exec_select(log, stmt);

}

int sqlite_get_all_rights(LOGGER log, sqlite3* db) {

	char* sql = "SELECT api_user, api_right FROM api_access";
	sqlite3_stmt* stmt = database_prepare(log, db, sql);

	if (!stmt) {
		return 2;
	}

	return sqlite_exec_select(log, stmt);
}

int sqlite_delete_rights(LOGGER log, sqlite3* db, const char* user) {

	char* sql = "DELETE FROM api_access WHERE api_user = ?";

        sqlite3_stmt* stmt = database_prepare(log, db, sql);
        if (!stmt) {
                return 2;
        }

	if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
       		logprintf(log, LOG_ERROR, "Cannot bind user.");
                sqlite3_finalize(stmt);
        	return 101;
        }

        sqlite3_finalize(stmt);
	if (sqlite3_changes(db) < 1) {
		printf("No user entries found.\n");
	}
        return 0;
}

int sqlite_delete_user_delegation(LOGGER log, sqlite3* db, const char* user, const char* delegate) {

	char* sql = "DELETE FROM api_user_delegates WHERE api_user = ? AND api_delegate = ?";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_INFO, "Cannot bind user.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_bind_text(stmt, 2, delegate, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_INFO, "Cannot bind delegate.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 5;
	}

	if (sqlite3_changes(db) < 1) {
		printf("User not found or user doesn't have this delegate.\n");
	}
	sqlite3_finalize(stmt);

	return 0;
}

int sqlite_delete_address_delegation(LOGGER log, sqlite3* db, const char* user, const char* expression) {

	char* sql = "DELETE FROM api_address_delegates WHERE api_user = ? AND api_expression = ?";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_INFO, "Cannot bind user.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_bind_text(stmt, 2, expression, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_INFO, "Cannot bind expression.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 5;
	}

	if (sqlite3_changes(db) < 1) {
		printf("User not found or user doesn't have this address space.\n");
	}
	sqlite3_finalize(stmt);

	return 0;
}
int sqlite_delete_right(LOGGER log, sqlite3* db, const char* user, const char* right) {

	char* sql = "DELETE FROM api_access WHERE api_user = ? AND api_right = ?";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_INFO, "Cannot bind user.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_bind_text(stmt, 2, right, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_INFO, "Cannot bind right.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 5;
	}

	if (sqlite3_changes(db) < 1) {
		printf("User not found or user doesn't have this right.\n");
	}
	sqlite3_finalize(stmt);

	return 0;
}

int sqlite_add_right(LOGGER log, sqlite3* db, const char* user, const char* right) {

        char* sql = "INSERT INTO api_access (api_user, api_right) VALUES (?, ?)";

        sqlite3_stmt* stmt = database_prepare(log, db, sql);
        if (!stmt) {
                return 2;
        }

        if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(log, LOG_ERROR, "Cannot bind username.\n");
                sqlite3_finalize(stmt);
                return 3;
        }

	if (sqlite3_bind_text(stmt, 2, right, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Cannot bind right\n");
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

int sqlite_delegate_address(LOGGER log, sqlite3* db, const char* user, const char* expression) {

        char* sql = "INSERT INTO api_address_delegates (api_user, api_expression) VALUES (?, ?)";

        sqlite3_stmt* stmt = database_prepare(log, db, sql);
        if (!stmt) {
                return 2;
        }

        if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(log, LOG_ERROR, "Cannot bind username.\n");
                sqlite3_finalize(stmt);
                return 3;
        }
	if (sqlite3_bind_text(stmt, 2, expression, -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(log, LOG_ERROR, "Cannot bind expression.\n");
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

int sqlite_delegate_user(LOGGER log, sqlite3* db, const char* user, const char* delegated) {

        char* sql = "INSERT INTO api_user_delegates (api_user, api_delegate) VALUES (?, ?)";

        sqlite3_stmt* stmt = database_prepare(log, db, sql);
        if (!stmt) {
                return 2;
        }

        if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(log, LOG_ERROR, "Cannot bind username.\n");
                sqlite3_finalize(stmt);
                return 3;
        }
	if (sqlite3_bind_text(stmt, 2, delegated, -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(log, LOG_ERROR, "Cannot bind delegate.\n");
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
