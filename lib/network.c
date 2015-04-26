ssize_t network_read(LOGGER log, CONNECTION* client, char* buffer, unsigned bytes){
	int status;

	#ifndef CMAIL_NO_TLS
	switch(client->tls_mode){
		case TLS_NONE:
			//non-tls client
			return recv(client->fd, buffer, bytes, 0);
		case TLS_NEGOTIATE:
			//tls handshake not completed
			status=gnutls_handshake(client->tls_session);
			if(status){
				if(gnutls_error_is_fatal(status)){
					logprintf(log, LOG_ERROR, "TLS Handshake reported fatal error: %s\n", gnutls_strerror(status));
					return -2;
				}
				logprintf(log, LOG_WARNING, "TLS Handshake reported nonfatal error: %s\n", gnutls_strerror(status));
				return -1;
			}

			logprintf(log, LOG_INFO, "TLS Handshake completed\n");
			return 0;
		case TLS_ONLY:
			//read with tls
			return gnutls_record_recv(client->tls_session, buffer, bytes);
	}
	#else
	return recv(client->fd, buffer, bytes, 0);
	#endif

	logprintf(log, LOG_ERROR, "Network read with invalid TLSMODE\n");
	return -2;
}

int network_connect(LOGGER log, char* host, uint16_t port){
	int sockfd=-1, error;
	char port_str[20];
	struct addrinfo hints;
	struct addrinfo* head;
	struct addrinfo* iter;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(port_str, sizeof(port_str), "%d", port);

	error=getaddrinfo(host, port_str, &hints, &head);
	if(error){
		logprintf(log, LOG_WARNING, "getaddrinfo: %s\r\n", gai_strerror(error));
		return -1;
	}

	for(iter=head;iter;iter=iter->ai_next){
		sockfd=socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol);
		if(sockfd<0){
			continue;
		}

		error=connect(sockfd, iter->ai_addr, iter->ai_addrlen);
		if(error!=0){
			close(sockfd);
			continue;
		}

		break;
	}

	freeaddrinfo(head);
	iter=NULL;

	if(sockfd<0){
		logprintf(log, LOG_WARNING, "Failed to create client socket: %s\n", strerror(errno));
		return -1;
	}

	if(error!=0){
		logprintf(log, LOG_WARNING, "Failed to create connect: %s\n", strerror(errno));
		return -1;
	}

	return sockfd;
}

int network_listener(LOGGER log, char* bindhost, char* port){
	int fd=-1, status, yes=1;
	struct addrinfo hints;
	struct addrinfo* info;
	struct addrinfo* addr_it;

	memset(&hints, 0, sizeof(hints));

	hints.ai_family=AF_UNSPEC;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_flags=AI_PASSIVE;

	status=getaddrinfo(bindhost, port, &hints, &info);
	if(status!=0){
		logprintf(log, LOG_ERROR, "Failed to get socket info for %s port %s: %s\n", bindhost, port, gai_strerror(status));
		return -1;
	}

	for(addr_it=info;addr_it!=NULL;addr_it=addr_it->ai_next){
		fd=socket(addr_it->ai_family, addr_it->ai_socktype, addr_it->ai_protocol);
		if(fd<0){
			continue;
		}

		if(setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (void*)&yes, sizeof(yes))<0){
			logprintf(log, LOG_WARNING, "Failed to set IPV6_V6ONLY on socket for %s port %s: %s\n", bindhost, port, strerror(errno));
		}

		yes=1;
		if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(yes))<0){
			logprintf(log, LOG_WARNING, "Failed to set SO_REUSEADDR on socket\n");
		}

		status=bind(fd, addr_it->ai_addr, addr_it->ai_addrlen);
		if(status<0){
			close(fd);
			continue;
		}

		break;
	}

	freeaddrinfo(info);

	if(!addr_it){
		logprintf(log, LOG_ERROR, "Failed to create listening socket for %s port %s\n", bindhost, port);
		return -1;
	}

	status=listen(fd, LISTEN_QUEUE_LENGTH);
	if(status<0){
		logprintf(log, LOG_ERROR, "Failed to listen on socket: %s", strerror(errno));
		close(fd);
		return -1;
	}

	return fd;
}
