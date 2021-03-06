int database_initialize(DATABASE* database){
	char* QUERY_OUTBOUND_HOSTS = "SELECT mail_remote, NULL AS mail_host, COUNT(mail_id) AS mail_amount FROM main.outbound WHERE mail_remote IS NOT NULL AND mail_fatality<1 AND (strftime('%s','now')-mail_lasttry)>?1 GROUP BY mail_remote UNION SELECT NULL AS mail_remote, SUBSTR(mail_envelopeto, INSTR(mail_envelopeto,'@')+1) AS mail_host, COUNT(mail_id) AS mail_amount FROM main.outbound WHERE mail_remote IS NULL AND mail_fatality<1 AND (strftime('%s','now')-mail_lasttry)>?1 GROUP BY SUBSTR(mail_envelopeto, INSTR(mail_envelopeto, '@')+1);";

	char* QUERY_OUTBOUND_MAIL_BY_DOMAIN = "SELECT GROUP_CONCAT(mail_id) AS mail_idlist, mail_envelopefrom, LENGTH(mail_data) AS mail_size, mail_data FROM main.outbound WHERE mail_fatality<1 AND (strftime('%s','now')-mail_lasttry)>? AND SUBSTR(mail_envelopeto,INSTR(mail_envelopeto,'@')+1)=?  AND mail_remote IS NULL GROUP BY mail_data, mail_envelopefrom;";
	char* QUERY_OUTBOUND_MAIL_BY_REMOTE = "SELECT GROUP_CONCAT(mail_id) AS mail_idlist, mail_envelopefrom, LENGTH(mail_data) AS mail_size, mail_data FROM main.outbound WHERE mail_fatality<1 AND (strftime('%s','now')-mail_lasttry)>? AND mail_remote=?  GROUP BY mail_data, mail_envelopefrom;";

	char* QUERY_RECIPIENT_PATH = "SELECT mail_envelopeto FROM main.outbox WHERE mail_id=?;";
	char* QUERY_BOUNCE_CANDIDATES="SELECT mail_id, mail_envelopefrom, mail_envelopeto, mail_submission, mail_failcount, mail_fatality, mail_data FROM main.outbound WHERE mail_fatality>0 OR mail_failcount>=?;";
	char* QUERY_BOUNCE_REASONS = "SELECT fail_time, fail_message, fail_fatal FROM main.faillog WHERE fail_mail = ?;";
	char* INSERT_BOUNCE_MESSAGE = "INSERT INTO main.outbox (mail_envelopeto, mail_data) VALUES (?, ? || ?);";
	char* INSERT_BOUNCE_REASON = "INSERT INTO main.faillog (fail_mail, fail_message, fail_fatal) VALUES (?, ?, ?);";
	char* DELETE_OUTBOUND_MAIL = "DELETE FROM main.outbox WHERE mail_id=?;";

	//check the database schema version
	if(database_schema_version(database->conn) != CMAIL_CURRENT_SCHEMA_VERSION){
		logprintf(LOG_ERROR, "The database schema is at another version than required for this build\n");
		return -1;
	}

	database->query_outbound_hosts = database_prepare(database->conn, QUERY_OUTBOUND_HOSTS);
	database->query_domain = database_prepare(database->conn, QUERY_OUTBOUND_MAIL_BY_DOMAIN);
	database->query_remote = database_prepare(database->conn, QUERY_OUTBOUND_MAIL_BY_REMOTE);
	database->query_rcpt = database_prepare(database->conn, QUERY_RECIPIENT_PATH);

	database->query_bounce_candidates = database_prepare(database->conn, QUERY_BOUNCE_CANDIDATES);
	database->query_bounce_reasons = database_prepare(database->conn, QUERY_BOUNCE_REASONS);
	database->insert_bounce = database_prepare(database->conn, INSERT_BOUNCE_MESSAGE);
	database->insert_bounce_reason = database_prepare(database->conn, INSERT_BOUNCE_REASON);
	database->delete_mail = database_prepare(database->conn, DELETE_OUTBOUND_MAIL);

	if(!database->query_outbound_hosts || !database->query_domain || !database->query_remote || !database->query_rcpt){
		logprintf(LOG_ERROR, "Failed to compile mail query statements\n");
		return -1;
	}

	if(!database->query_bounce_candidates || !database->query_bounce_reasons || !database->insert_bounce || !database->insert_bounce_reason){
		logprintf(LOG_ERROR, "Failed to compile bounce statements\n");
		return -1;
	}

	if(!database->delete_mail){
		logprintf(LOG_ERROR, "Failed to compile mail management statements\n");
		return -1;
	}

	return 0;
}

void database_free(DATABASE* database){
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

		sqlite3_close(database->conn);
		database->conn = NULL;
	}
}
