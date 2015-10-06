MAILPATH* pathpool_get(LOGGER log, PATHPOOL* pool){
	unsigned i;

	if(!pool || (!pool->paths && pool->count != 0)){
		return NULL;
	}

	//find free path
	for(i = 0; i < pool->count; i++){
		if(!pool->paths[i]->in_transaction){
			pool->paths[i]->in_transaction = true;
			logprintf(log, LOG_INFO, "Reusing path pool slot %d\n", i);
			return pool->paths[i];
		}
	}

	//reallocate if needed
	if(pool->count < CMAIL_MAX_POOLED_PATHS){
		pool->paths = realloc(pool->paths, sizeof(MAILPATH*) * (pool->count + 1));
		if(!pool->paths){
			logprintf(log, LOG_ERROR, "Failed to reallocate path pool to %d entries\n", pool->count + 1);
			return NULL;
		}

		pool->paths[pool->count] = calloc(1, sizeof(MAILPATH));
		if(!pool->paths[pool->count]){
			logprintf(log, LOG_ERROR, "Failed to allocate path pool entry\n");
			return NULL;
		}

		path_reset(pool->paths[pool->count]);
		logprintf(log, LOG_DEBUG, "Reallocated path pool to %d entries\n", pool->count + 1);
		pool->paths[pool->count]->in_transaction = true;

		return pool->paths[pool->count++];
	}
	else{
		logprintf(log, LOG_WARNING, "Path pool exhausted\n");
		return NULL;
	}
}

void pathpool_return(MAILPATH* path){
	if(!path){
		return;
	}

	path_reset(path);

	//redundant, but oh well
	path->in_transaction = false;
}

void pathpool_free(PATHPOOL* pool){
	unsigned i;

	if(!pool){
		return;
	}

	if(pool->paths){
		for(i = 0; i < pool->count; i++){
			path_reset(pool->paths[i]);
			free(pool->paths[i]);
			pool->paths[i] = NULL;
		}
		free(pool->paths);
	}

	pool->paths = NULL;
	pool->count = 0;
}
