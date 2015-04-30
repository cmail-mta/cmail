int database_initialize(LOGGER log, DATABASE* database){
	char* QUERY_OUTBOUND_HOSTS="SELECT mail_remote, NULL AS mail_host FROM main.outbox WHERE mail_remote IS NOT NULL GROUP BY mail_remote UNION SELECT NULL AS mail_remote, SUBSTR( mail_envelopeto, INSTR( mail_envelopeto, '@' ) + 1 ) AS mail_host FROM main.outbox WHERE mail_remote IS NULL GROUP BY SUBSTR( mail_envelopeto, INSTR( mail_envelopeto, '@' ) + 1 );";

	//TODO fetch only mail within retry interval range
	char* QUERY_OUTBOUND_MAIL_BY_DOMAIN="SELECT GROUP_CONCAT(mail_id) AS mail_idlist, LENGTH(mail_data) AS mail_size, mail_data FROM main.outbox WHERE SUBSTR( mail_envelopeto, INSTR( mail_envelopeto, '@' ) + 1 )=? GROUP BY mail_data, mail_envelopefrom;";
	char* QUERY_OUTBOUND_MAIL_BY_REMOTE="SELECT GROUP_CONCAT(mail_id) AS mail_idlist, LENGTH(mail_data) AS mail_size, mail_data FROM main.outbox WHERE mail_remote=? GROUP BY mail_data, mail_envelopefrom;";

	//check the database schema version
	if(database_schema_version(log, database->conn)!=CMAIL_CURRENT_SCHEMA_VERSION){
		logprintf(log, LOG_ERROR, "The database schema is at another version than required for this build\n");
		return -1;
	}

	database->query_outbound_hosts=database_prepare(log, database->conn, QUERY_OUTBOUND_HOSTS);
	database->query_domain=database_prepare(log, database->conn, QUERY_OUTBOUND_MAIL_BY_DOMAIN);
	database->query_remote=database_prepare(log, database->conn, QUERY_OUTBOUND_MAIL_BY_REMOTE);

	if(!database->query_outbound_hosts || !database->query_domain || !database->query_remote){
		logprintf(log, LOG_ERROR, "Failed to create mail query statement\n");
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
		sqlite3_close(database->conn);
		database->conn=NULL;
	}
}
