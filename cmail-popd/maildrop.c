int maildrop_acquire(LOGGER log, DATABASE* database, MAILDROP* maildrop){
	//atomically lock maildrop, bail out if it fails
	//read mail data from master
	//read mail data from user table if needed
	return -1;
}

int maildrop_update(LOGGER log, DATABASE* database, MAILDROP* maildrop){
	//TODO
	return -1;
}

int maildrop_release(LOGGER log, DATABASE* database, MAILDROP* maildrop){
	//TODO
	return -1;
}
