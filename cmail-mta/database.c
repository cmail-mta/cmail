int database_initialize(LOGGER log, DATABASE* database){
	char* QUERY_OUTBOUND_HOSTS="SELECT mail_remote, NULL AS mail_host FROM main.outbox WHERE mail_remote IS NOT NULL GROUP BY mail_remote UNION SELECT NULL AS mail_remote, SUBSTR( mail_envelopeto, INSTR( mail_envelopeto, '@' ) + 1 ) AS mail_host FROM main.outbox WHERE mail_remote IS NULL GROUP BY SUBSTR( mail_envelopeto, INSTR( mail_envelopeto, '@' ) + 1 );";

	//TODO fetch only mail within retry interval range
	char* QUERY_OUTBOUND_MAIL_BY_DOMAIN="SELECT GROUP_CONCAT(mail_id) AS mail_idlist, mail_envelopefrom, LENGTH(mail_data) AS mail_size, mail_data FROM main.outbox WHERE SUBSTR( mail_envelopeto, INSTR( mail_envelopeto, '@' ) + 1 )=? GROUP BY mail_data, mail_envelopefrom;";
	char* QUERY_OUTBOUND_MAIL_BY_REMOTE="SELECT GROUP_CONCAT(mail_id) AS mail_idlist, mail_envelopefrom, LENGTH(mail_data) AS mail_size, mail_data FROM main.outbox WHERE mail_remote=? GROUP BY mail_data, mail_envelopefrom;";

	char* QUERY_RECIPIENT_PATH="SELECT mail_envelopeto FROM main.outbox WHERE mail_id=?;";
	char* QUERY_BOUNCE_CANDIDATES="SELECT mail_id, mail_envelopefrom, mail_submission, mail_attempts, mail_data FROM main.outbox WHERE mail_attempts > ?;";
	char* QUERY_BOUNCE_REASONS="SELECT 1;"; //TODO
	char* INSERT_BOUNCE_MESSAGE="SELECT 1;"; //TODO
	char* INSERT_BOUNCE_REASON="SELECT 1;"; //TODO
	char* DELETE_OUTBOUND_MAIL="DELETE FROM main.outbox WHERE mail_id=?;";
	char* UPDATE_MAIL_ATTEMPTS="UPDATE main.outbox SET mail_attempts=mail_attempts+1, mail_lastattempt=strftime('%s', 'now') WHERE mail_id=?;";

	//check the database schema version
	if(database_schema_version(log, database->conn)!=CMAIL_CURRENT_SCHEMA_VERSION){
		logprintf(log, LOG_ERROR, "The database schema is at another version than required for this build\n");
		return -1;
	}

	database->query_outbound_hosts=database_prepare(log, database->conn, QUERY_OUTBOUND_HOSTS);
	database->query_domain=database_prepare(log, database->conn, QUERY_OUTBOUND_MAIL_BY_DOMAIN);
	database->query_remote=database_prepare(log, database->conn, QUERY_OUTBOUND_MAIL_BY_REMOTE);
	database->query_rcpt=database_prepare(log, database->conn, QUERY_RECIPIENT_PATH);

	database->query_bounce_candidates=database_prepare(log, database->conn, QUERY_BOUNCE_CANDIDATES);
	database->query_bounce_reasons=database_prepare(log, database->conn, QUERY_BOUNCE_REASONS);
	database->insert_bounce=database_prepare(log, database->conn, INSERT_BOUNCE_MESSAGE);
	database->insert_bounce_reason=database_prepare(log, database->conn, INSERT_BOUNCE_REASON);
	database->delete_mail=database_prepare(log, database->conn, DELETE_OUTBOUND_MAIL);
	database->update_failcount=database_prepare(log, database->conn, UPDATE_MAIL_ATTEMPTS);

	if(!database->query_outbound_hosts || !database->query_domain || !database->query_remote || !database->query_rcpt){
		logprintf(log, LOG_ERROR, "Failed to compile mail query statements\n");
		return -1;
	}

	if(!database->query_bounce_candidates || !database->query_bounce_reasons || !database->insert_bounce || !database->insert_bounce_reason){
		logprintf(log, LOG_ERROR, "Failed to compile bounce statements\n");
		return -1;
	}

	if(!database->delete_mail || !database->update_failcount){
		logprintf(log, LOG_ERROR, "Failed to compile mail management statements\n");
		return -1;
	}

	return 0;
}

void database_free(LOGGER log, DATABASE* database){
	//FIXME check for SQLITE_BUSY here

	if(database->conn){
		sqlite3_finalize(database->query_outbound_hosts);
		sqlite3_finalize(database->query_remote);
		sqlite3_finalize(database->query_domain);
		sqlite3_finalize(database->query_rcpt);

		sqlite3_finalize(database->query_bounce_candidates);
		sqlite3_finalize(database->query_bounce_reasons);
		sqlite3_finalize(database->insert_bounce);
		sqlite3_finalize(database->insert_bounce_reason);
		sqlite3_finalize(database->delete_mail);
		sqlite3_finalize(database->update_failcount);

		sqlite3_close(database->conn);
		database->conn=NULL;
	}
}
