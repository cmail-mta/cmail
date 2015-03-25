int database_initialize(LOGGER log, DATABASE* database){
	//TODO compile statements
	return 0;
}

void database_free(LOGGER log, DATABASE* database){
	//FIXME check for SQLITE_BUSY here
	if(database->conn){
		sqlite3_close(database->conn);
		database->conn=NULL;
	}
}
