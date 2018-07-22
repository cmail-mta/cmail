int sqlite_exec_select(sqlite3_stmt* stmt) {

	int status;
	const char* user;
	const char* permission;

	printf("%20s | %10s\n%s\n", "User", "Permission", "---------------------+-----------");

	while ((status = sqlite3_step(stmt)) == SQLITE_ROW) {

		user = (const char*) sqlite3_column_text(stmt, 0);
		permission = (const char*) sqlite3_column_text(stmt, 1);
		printf("%20s | %10s\n", user, permission);
	}

	sqlite3_finalize(stmt);
	if (status == SQLITE_DONE) {
		return 0;
	}

	return status;
}

int sqlite_get_delegated_users(sqlite3* db, const char* username) {

	char* sql = "SELECT api_user, api_delegate FROM api_user_delegates WHERE api_user LIKE ?";

	sqlite3_stmt* stmt = database_prepare(db, sql);

	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_ERROR, "Cannot bind expression.\n");
		return 3;
	}

	return sqlite_exec_select(stmt);

}

int sqlite_get_delegated_addresses(sqlite3* db, const char* username) {

	char* sql = "SELECT api_user, api_expression FROM api_address_delegates WHERE api_user LIKE ?";

	sqlite3_stmt* stmt = database_prepare(db, sql);

	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_ERROR, "Cannot bind expression.\n");
		return 3;
	}

	return sqlite_exec_select(stmt);
}

int sqlite_get_delegated(sqlite3* db, const char* username) {

	printf("Delegated users:\n");
	int status = sqlite_get_delegated_users(db, username);

	if (status > 0) {
		return status;
	}

	printf("\nDelegated address space:\n");
	return sqlite_get_delegated_addresses(db, username);
}

int sqlite_get_permissions(sqlite3* db, const char* username) {


	char* sql = "SELECT api_user, api_permission FROM api_access WHERE api_user LIKE ?";

	sqlite3_stmt* stmt = database_prepare(db, sql);

	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_ERROR, "Cannot bind expression.\n");
		return 3;
	}

	return sqlite_exec_select(stmt);
}

int sqlite_get_permissions_by_permission(sqlite3* db, const char* permission) {

	char*  sql = "SELECT api_user, api_permission FROM api_access WHERE api_permission LIKE ?";

	sqlite3_stmt* stmt = database_prepare(db, sql);

	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, permission, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_ERROR, "Cannot bind permission.\n");
		return 3;
	}

	return sqlite_exec_select(stmt);

}

int sqlite_delete_permissions(sqlite3* db, const char* user) {

	char* sql = "DELETE FROM api_access WHERE api_user = ?";

        sqlite3_stmt* stmt = database_prepare(db, sql);
        if (!stmt) {
                return 2;
        }

	if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
       		logprintf(LOG_ERROR, "Cannot bind user.");
                sqlite3_finalize(stmt);
        	return 101;
        }

        sqlite3_finalize(stmt);
	if (sqlite3_changes(db) < 1) {
		printf("No user entries found.\n");
	}
        return 0;
}

int sqlite_delete_user_delegation(sqlite3* db, const char* user, const char* delegate) {

	char* sql = "DELETE FROM api_user_delegates WHERE api_user = ? AND api_delegate = ?";

	sqlite3_stmt* stmt = database_prepare(db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_INFO, "Cannot bind user.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_bind_text(stmt, 2, delegate, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_INFO, "Cannot bind delegate.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 5;
	}

	if (sqlite3_changes(db) < 1) {
		printf("User not found or user doesn't have this delegate.\n");
	}
	sqlite3_finalize(stmt);

	return 0;
}

int sqlite_delete_address_delegation(sqlite3* db, const char* user, const char* expression) {

	char* sql = "DELETE FROM api_address_delegates WHERE api_user = ? AND api_expression = ?";

	sqlite3_stmt* stmt = database_prepare(db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_INFO, "Cannot bind user.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_bind_text(stmt, 2, expression, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_INFO, "Cannot bind expression.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 5;
	}

	if (sqlite3_changes(db) < 1) {
		printf("User not found or user doesn't have this address space.\n");
	}
	sqlite3_finalize(stmt);

	return 0;
}
int sqlite_delete_permission(sqlite3* db, const char* user, const char* permission) {

	char* sql = "DELETE FROM api_access WHERE api_user = ? AND api_permission = ?";

	sqlite3_stmt* stmt = database_prepare(db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_INFO, "Cannot bind user.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_bind_text(stmt, 2, permission, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_INFO, "Cannot bind permission.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 5;
	}

	if (sqlite3_changes(db) < 1) {
		printf("User not found or user doesn't have this permission.\n");
	}
	sqlite3_finalize(stmt);

	return 0;
}

int sqlite_add_permission(sqlite3* db, const char* user, const char* permission) {

        char* sql = "INSERT INTO api_access (api_user, api_permission) VALUES (?, ?)";

        sqlite3_stmt* stmt = database_prepare(db, sql);
        if (!stmt) {
                return 2;
        }

        if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(LOG_ERROR, "Cannot bind username.\n");
                sqlite3_finalize(stmt);
                return 3;
        }

	if (sqlite3_bind_text(stmt, 2, permission, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(LOG_ERROR, "Cannot bind permission\n");
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

int sqlite_delegate_address(sqlite3* db, const char* user, const char* expression) {

        char* sql = "INSERT INTO api_address_delegates (api_user, api_expression) VALUES (?, ?)";

        sqlite3_stmt* stmt = database_prepare(db, sql);
        if (!stmt) {
                return 2;
        }

        if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(LOG_ERROR, "Cannot bind username.\n");
                sqlite3_finalize(stmt);
                return 3;
        }
	if (sqlite3_bind_text(stmt, 2, expression, -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(LOG_ERROR, "Cannot bind expression.\n");
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

int sqlite_delegate_user(sqlite3* db, const char* user, const char* delegated) {

        char* sql = "INSERT INTO api_user_delegates (api_user, api_delegate) VALUES (?, ?)";

        sqlite3_stmt* stmt = database_prepare(db, sql);
        if (!stmt) {
                return 2;
        }

        if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(LOG_ERROR, "Cannot bind username.\n");
                sqlite3_finalize(stmt);
                return 3;
        }
	if (sqlite3_bind_text(stmt, 2, delegated, -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(LOG_ERROR, "Cannot bind delegate.\n");
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
