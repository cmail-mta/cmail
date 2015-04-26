int connpool_add(CONNPOOL* pool, int fd){
	unsigned i;
	int free_slot;
	CONNECTION new_connection = {
		#ifndef CMAIL_NO_TLS
		.tls_mode=TLS_NONE,
		#endif
		.fd = -1,
		.aux_data = NULL
	};

	if(fd<0||!pool){
		return -1;
	}

	new_connection.fd = fd;

	//initialize if needed
	if(!pool->conns){
		pool->conns=malloc(sizeof(CONNECTION));
		if(!pool->conns){
			return -127;
		}

		pool->conns[0]=new_connection;
		pool->count=1;
		return 0;
	}

	//check if already in set, search for free slots at same time
	free_slot=-1;
	for(i=0;i<pool->count;i++){
		if(pool->conns[i].fd==fd){
			return i;
		}
		if(pool->conns[i].fd==-1){
			free_slot=i;
			new_connection.aux_data=pool->conns[free_slot].aux_data;
			break;
		}
	}

	//reallocate if needed
	if(free_slot<0){
		pool->conns=realloc(pool->conns, sizeof(CONNECTION)*((pool->count)+1));
		if(!pool->conns){
			return -127;
		}

		free_slot=pool->count++;
	}

	pool->conns[free_slot]=new_connection;

	return free_slot;
}

int connpool_remove(CONNPOOL* pool, int fd){
	unsigned i;

	if(fd<0||!pool||!(pool->conns)){
		return -1;
	}

	for(i=0;i<pool->count;i++){
		if(pool->conns[i].fd==fd){
			pool->conns[i].fd=-1;
			return 0;
		}
	}

	return 1;
}

int connpool_active(CONNPOOL pool){
	unsigned i, count=0;

	for(i=0;i<pool.count;i++){
		if(pool.conns[i].fd>=0){
			count++;
		}
	}

	return count;
}

void connpool_free(CONNPOOL* pool){
	unsigned i;

	if(!pool){
		return;
	}

	if(pool->conns){
		for(i=0;i<pool->count;i++){
			if(pool->conns[i].fd>=0){
				close(pool->conns[i].fd);
			}

			if(pool->conns[i].aux_data){
				free(pool->conns[i].aux_data);
			}
		}

		free(pool->conns);
		pool->conns=NULL;
	}

	pool->count=0;
}
