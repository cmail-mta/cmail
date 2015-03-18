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
