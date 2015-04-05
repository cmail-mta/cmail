int database_initialize(LOGGER log, DATABASE* database){
	char* QUERY_OUTBOUND_MAIL="SELECT GROUP_CONCAT(mail_id) AS mail_idlist, mail_remote, SUBSTR(mail_envelopeto, INSTR(mail_envelopeto, '@')+1) AS mail_host, GROUP_CONCAT(mail_envelopeto) AS mail_recipients, LENGTH(mail_data) AS mail_size, mail_data FROM main.outbox GROUP BY mail_remote, SUBSTR(mail_envelopeto, INSTR(mail_envelopeto, '@')+1), mail_data, mail_envelopefrom;";
	
	//check the database schema version
	if(database_schema_version(log, database->conn)!=CMAIL_CURRENT_SCHEMA_VERSION){
		logprintf(log, LOG_ERROR, "The database schema is at another version than required for this build\n");
		return -1;
	}
	
	database->query_outbound=database_prepare(log, database->conn, QUERY_OUTBOUND_MAIL);

	if(!database->query_outbound){
		logprintf(log, LOG_ERROR, "Failed to create mail query statement\n");
		return -1;
	}

	return 0;
}

void database_free(LOGGER log, DATABASE* database){
	//FIXME check for SQLITE_BUSY here
	
	if(database->conn){
		sqlite3_finalize(database->query_outbound);
		sqlite3_close(database->conn);
		database->conn=NULL;
	}
}
