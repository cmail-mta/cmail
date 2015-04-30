int mail_dbread(LOGGER log, MAIL* mail, sqlite3_stmt* stmt){
	int entries=1;
	unsigned i;
	char* id_list=(char*)sqlite3_column_text(stmt, 0);
	
	for(i=0;i<strlen(id_list);i++){
		if(id_list[i]==','){
			entries++;
		}
	}

	logprintf(log, LOG_DEBUG, "%d id list entries in %s\n", entries, id_list);

	mail->ids=calloc(entries+1, sizeof(int));
	if(!mail->ids){
		logprintf(log, LOG_ERROR, "Failed to allocate memory for ID list\n");
		return -1;
	}
	mail->ids[entries]=-1;

	i=0;
	do{

		mail->ids[i]=strtoul(id_list, &id_list, 10);
		logprintf(log, LOG_DEBUG, "Updated entry %d to id %d, nextptr is at %s\n", i, mail->ids[i], id_list);
		i++;
		id_list++;
	}
	while(i<entries);

	mail->length=sqlite3_column_int(stmt, 1);
	logprintf(log, LOG_DEBUG, "%d bytes of mail data\n", mail->length);

	//read maildata
	mail->data=common_strdup((char*)sqlite3_column_text(stmt, 2));
	if(!mail->data){
		logprintf(log, LOG_ERROR, "Failed to allocate memory for mail data\n");
		return -1;
	}

	return 0;
}

int mail_dispatch(LOGGER log, DATABASE* database, MAIL* mail){
	return -1;
}

int mail_free(MAIL* mail){
	MAIL empty_mail = {
		.ids = NULL,
		.length = 0,
		.data = NULL
	};

	if(mail->ids){
		free(mail->ids);
	}

	if(mail->data){
		free(mail->data);
	}

	*mail=empty_mail;
	return 0;
}
