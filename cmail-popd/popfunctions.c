int pop_capa(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data=(CLIENT*)client->aux_data;

	logprintf(log, LOG_DEBUG, "Client requested capability listing\n");

	client_send(log, client, "+OK Capability listing\r\n");
	//FIXME allow tls-only auth
	//TODO XYZZY
	client_send(log, client, "USER\r\n");
	client_send(log, client, "SASL LOGIN\r\n");
	//FIXME do not announce when already in tls or no tls possible
	//client_send(log, client, "STLS\r\n");
	client_send(log, client, "IMPLEMENTATION %s\r\n", VERSION);
	client_send(log, client, "XYZZY\r\n", VERSION);
	client_send(log, client, ".\r\n", VERSION);

	return 0;
}

int pop_quit(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	LISTENER* listener_data=(LISTENER*)client_data->listener->aux_data;
	
	if(client_data->state==STATE_TRANSACTION){
		//TODO execute the update
		client_send(log, client, "+OK %s POP3 done\r\n", listener_data->announce_domain);
	}
	else{
		client_send(log, client, "+OK %s POP3 done\r\n", listener_data->announce_domain);
	}

	return client_close(client);
}

int pop_xyzzy(LOGGER log, CONNECTION* client, DATABASE* database){
	logprintf(log, LOG_INFO, "Client performs incantation\n");
	client_send(log, client, "+OK Nothing happens\r\n");
	return 0;
}
