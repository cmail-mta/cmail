int fdcollection_add(FDCOLLECTION* coll, int fd){
	unsigned i;
	int free_slot;

	if(fd<0||!coll){
		return -1;
	}

	//initialize if needed
	if(!coll->fds){
		coll->fds=malloc(sizeof(int));
		if(!coll->fds){
			return -127;
		}
		
		coll->fds[0]=fd;
		coll->count=1;
		return 0;
	}

	//check if already in set, search for free slots at same time
	free_slot=-1;
	for(i=0;i<coll->count;i++){
		if(coll->fds[i]==fd){
			return 1;
		}
		if(coll->fds[i]==-1){
			free_slot=i;
		}
	}

	//reallocate if needed
	if(free_slot<0){
		coll->fds=realloc(coll->fds, sizeof(int)*(coll->count+1));
		if(!coll->fds){
			return -127;
		}

		free_slot=coll->count++;
	}

	coll->fds[free_slot]=fd;
	return 0;
}

int fdcollection_remove(FDCOLLECTION* coll, int fd){
	unsigned i;

	if(fd<0||!coll||!(coll->fds)){
		return -1;
	}

	for(i=0;i<coll->count;i++){
		if(coll->fds[i]==fd){
			coll->fds[i]=-1;
			return 0;
		}
	}

	return 1;
}

void fdcollection_free(FDCOLLECTION* coll){
	if(!coll){
		return;
	}

	if(coll->fds){
		free(coll->fds);
		coll->fds=NULL;
	}
	coll->count=0;
}
