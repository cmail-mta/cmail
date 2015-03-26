int state_authorization(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	
	if(!strncasecmp(client_data->recv_buffer, "capa", 4)){
		return pop_capa(log, client, database);
	}

	if(!strncasecmp(client_data->recv_buffer, "quit", 4)){
		return pop_quit(log, client, database);
	}

	return -1;
}

int state_transaction(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	
	if(!strncasecmp(client_data->recv_buffer, "capa", 4)){
		return pop_capa(log, client, database);
	}

	if(!strncasecmp(client_data->recv_buffer, "quit", 4)){
		return pop_quit(log, client, database);
	}

	return -1;
}

int state_update(LOGGER log, CONNECTION* client, DATABASE* database){
	//this should not be reached
	//TODO return error
	return -1;
}
