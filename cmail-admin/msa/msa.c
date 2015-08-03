int sqlite_get_msa(LOGGER log, sqlite3* db, const char* username) {


	char* sql = "SELECT msa_user, msa_inrouter, msa_inroute, msa_outrouter, msa_outroute FROM msa WHERE msa_user LIKE ?";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);

	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_ERROR, "Cannot bind expression.\n");
		return 3;
	}

	int status;
	const char* user;
	const char* inrouter;
	const char* inroute;
	const char* outrouter;
	const char* outroute;

	printf("%20s | %10s | %15s | %10s | %15s\n%s\n", "User", "inrouter", "inroute", "outrouter", "outroute",
			"---------------------+------------+-----------------+------------+----------------");

	while ((status = sqlite3_step(stmt)) == SQLITE_ROW) {

		user = (const char*) sqlite3_column_text(stmt, 0);
		inrouter = (const char*) sqlite3_column_text(stmt, 1);
		inroute = (const char*) sqlite3_column_text(stmt, 2);
		outrouter = (const char*) sqlite3_column_text(stmt, 3);
		outroute = (const char*) sqlite3_column_text(stmt, 4);
		printf("%20s | %10s | %15s | %10s | %15s\n", user, inrouter, inroute, outrouter, outroute);
	}

	sqlite3_finalize(stmt);
	if (status == SQLITE_DONE) {
		return 0;
	}

	return status;
}

int sqlite_get_all_msa(LOGGER log, sqlite3* db) {

	char* sql = "SELECT msa_user, msa_inrouter, msa_inroute, msa_outrouter, msa_outroute FROM msa";
	sqlite3_stmt* stmt = database_prepare(log, db, sql);

	if (!stmt) {
		return 2;
	}
	int status;
	const char* user;
	const char* inrouter;
	const char* inroute;
	const char* outrouter;
	const char* outroute;

	while ((status = sqlite3_step(stmt)) == SQLITE_ROW) {

		user = (const char*) sqlite3_column_text(stmt, 0);
		inrouter = (const char*) sqlite3_column_text(stmt, 1);
		inroute = (const char*) sqlite3_column_text(stmt, 2);
		outrouter = (const char*) sqlite3_column_text(stmt, 3);
		outroute = (const char*) sqlite3_column_text(stmt, 4);
		printf("%s | %s | %s | %s | %s\n", user, inrouter, inroute, outrouter, outroute);
	}


	sqlite3_finalize(stmt);

	if (status == SQLITE_DONE) {
		return 0;
	}
	return status;
}

int sqlite_update_msa(LOGGER log, sqlite3* db, const char* router, const char* user, const char* rtype, const char* argument) {

	char* sql;

	if (!strcmp(router, "inrouter")) {

		sql = "UPDATE msa SET msa_inrouter = ?, msa_inroute = ? WHERE msa_user = ?";
	} else if (!strcmp(router, "outrouter")) {
		sql = "UPDATE msa SET msa_outrouter = ?, msa_outroute = ? WHERE msa_user = ?";
	} else {
		logprintf(log, LOG_ERROR, "No valid router (must be \"inrouter\" or \"outrouter\".");
		return 70;
	}

        sqlite3_stmt* stmt = database_prepare(log, db, sql);
        if (!stmt) {
                return 2;
        }

        if (sqlite3_bind_text(stmt, 1, rtype , -1, SQLITE_STATIC) != SQLITE_OK) {
                logprintf(log, LOG_ERROR, "Cannot bind router type.\n");
                sqlite3_finalize(stmt);
                return 71;
        }

	if (argument) {
       		if (sqlite3_bind_text(stmt, 2, argument, -1, SQLITE_STATIC) != SQLITE_OK) {
       			logprintf(log, LOG_ERROR, "Cannot bind argument.");
        	        sqlite3_finalize(stmt);
        		return 72;
        	}
	} else {
		sqlite3_bind_null(stmt, 2);
	}

	if (sqlite3_bind_text(stmt, 3, user, -1, SQLITE_STATIC) != SQLITE_OK) {
       		logprintf(log, LOG_ERROR, "Cannot bind user.");
                sqlite3_finalize(stmt);
        	return 73;
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
                logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
		return 5;
        }
        sqlite3_finalize(stmt);
	if (sqlite3_changes(db) < 1) {
		printf("MSA entry not found.\n");
	}
        return 0;
}

int sqlite_delete_msa(LOGGER log, sqlite3* db, const char* user) {

	char* sql = "DELETE FROM msa WHERE msa_user = ?";

	sqlite3_stmt* stmt = database_prepare(log, db, sql);
	if (!stmt) {
		return 2;
	}

	if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
		logprintf(log, LOG_INFO, "Cannot bind msa.\n");
		sqlite3_finalize(stmt);
		return 3;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		logprintf(log, LOG_ERROR, "%s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 5;
	}

	if (sqlite3_changes(db) < 1) {
		printf("MSA entry not found.\n");
	}
	sqlite3_finalize(stmt);

	return 0;
}

int sqlite_add_address_order(LOGGER log, sqlite3* db, const char* expression, const char* user, unsigned order) {

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

int sqlite_add_msa_default(LOGGER log, sqlite3* db, const char* user) {

        char* sql = "INSERT INTO msa (msa_user) VALUES (?)";

        sqlite3_stmt* stmt = database_prepare(log, db, sql);
        if (!stmt) {
                return 2;
        }

        if (sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC) != SQLITE_OK) {
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
