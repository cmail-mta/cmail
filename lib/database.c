void database_instr(sqlite3_context* context, int argc, sqlite3_value** argv){
	//mostly copied from sqlite-src/func.c
	
	int nHaystack;
	int nNeedle;
	int N = 1;

	const unsigned char* hay;
	const unsigned char* needle;
	bool text=false;

	if(sqlite3_value_type(argv[0])==SQLITE_NULL
		|| sqlite3_value_type(argv[1])==SQLITE_NULL){
		//bail out
		return;
	}

	nHaystack = sqlite3_value_bytes(argv[0]);
	nNeedle = sqlite3_value_bytes(argv[1]);
	
	if(sqlite3_value_type(argv[0])==SQLITE_BLOB
		&& sqlite3_value_type(argv[1])==SQLITE_BLOB){

		hay=sqlite3_value_blob(argv[0]);
		needle=sqlite3_value_blob(argv[1]);
  	}
	else{
		hay=sqlite3_value_text(argv[0]);
		needle=sqlite3_value_text(argv[1]);
		text=true;
	}
	
	while(nNeedle<=nHaystack && memcmp(hay, needle, nNeedle)!=0 ){
		N++;
		do{
			nHaystack--;
			hay++;
		}
		while(text&&(hay[0]&0xc0)==0x80);
	}
  
	if(nNeedle>nHaystack){
		N=0;
	}
	sqlite3_result_int(context, N);
}

sqlite3_stmt* database_prepare(LOGGER log, sqlite3* conn, char* query){
	int status;
	sqlite3_stmt* target=NULL;

	status=sqlite3_prepare_v2(conn, query, strlen(query), &target, NULL);

	switch(status){
		case SQLITE_OK:
			logprintf(log, LOG_DEBUG, "Statement (%s) compiled ok\n", query);
			return target;
		default:
			logprintf(log, LOG_ERROR, "Failed to prepare statement (%s): %s\n", query, sqlite3_errmsg(conn));
	}

	return NULL;
}

int database_schema_version(LOGGER log, sqlite3* conn){
	int status;
	char* QUERY_SCHEMA_VERSION="SELECT value FROM meta WHERE key='schema_version'";
	sqlite3_stmt* query_version=database_prepare(log, conn, QUERY_SCHEMA_VERSION);

	if(!query_version){
		logprintf(log, LOG_ERROR, "Failed to query the database schema version, your database file might not be a valid master database\n");
		return -1;
	}

	switch(sqlite3_step(query_version)){
		case SQLITE_ROW:
			status=strtoul((char*)sqlite3_column_text(query_version, 0), NULL, 10);
			logprintf(log, LOG_INFO, "Database schema version is %d\n", status);
			break;
		case SQLITE_DONE:
			logprintf(log, LOG_ERROR, "The database did not contain a schema_version key\n");
			status=-1;
			break;
		default:
			logprintf(log, LOG_ERROR, "Failed to get schema version: %s\n", sqlite3_errmsg(conn));
			status=-1;
	}

	sqlite3_finalize(query_version);
	return status;
}
