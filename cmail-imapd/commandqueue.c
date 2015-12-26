void commandqueue_reset_entry(QUEUED_COMMAND* entry, bool keep_allocations){
	size_t i;

	QUEUED_COMMAND empty = {
		.active = false,
		.next = NULL,
		.prev = NULL,
		
		.queue_state = COMMAND_NEW,
		.last_enqueue = 0,
		.backing_buffer = (keep_allocations) ? entry->backing_buffer:NULL,
		.backing_buffer_length = (keep_allocations) ? entry->backing_buffer_length:0,
		
		.tag = NULL,
		.command = NULL,
		.parameters = (keep_allocations) ? entry->parameters:NULL,
		.parameters_length = (keep_allocations) ? entry->parameters_length:0,

		.replies = (keep_allocations) ? entry->replies:NULL,
		.replies_length = (keep_allocations) ? entry->replies_length:0
	};

	if(keep_allocations){
		if(empty.backing_buffer){
			empty.backing_buffer[0] = 0;
		}

		if(empty.replies){
			empty.replies[0] = 0;
		}

		for(i = 0; i < empty.parameters_length; i++){
			empty.parameters[i] = NULL;
		}
	}

	*entry = empty;
	return;
}

int commandqueue_initialize(LOGGER log, COMMAND_QUEUE* command_queue){
	size_t i;

	//allocate initial queue entry buffer
	command_queue->entries_length = CMAIL_IMAP_QUEUE_INITIAL;
	command_queue->entries = calloc(CMAIL_IMAP_QUEUE_INITIAL, sizeof(QUEUED_COMMAND));
	for(i = 0; i < command_queue->entries_length; i++){
		commandqueue_reset_entry(command_queue->entries + i, false);
	}
	return 0;
}

int commandqueue_enqueue(LOGGER log, COMMAND_QUEUE* queue, IMAP_COMMAND command, char** parameters){
	size_t entry;
	int rv = 0;

	//find empty entry
	//this works because only the client handler thread is allowed to update the entry active flag
	for(entry = 0; entry < queue->entries_length; entry++){
		if(!queue->entries[entry].active){
			//found a slot
			break;
		}
	}

	if(entry >= queue->entries_length){
		//reallocate queue
		logprintf(log, LOG_WARNING, "Command queue needs reallocation\n");
		pthread_mutex_lock(&(queue->queue_access));
		//TODO this can only happen if no entry is currently IN_PROGRESS in order to avoid dangling pointers
		//TODO this also needs to update all next/previous/head/tail pointers
		//TODO reallocate entries array
		pthread_mutex_unlock(&(queue->queue_access));

		//FIXME remove this temporary bailout
		return -1;
	}

	//update bookkeeping data
	queue->entries[entry].active = true;
	queue->entries[entry].last_enqueue = time(NULL);
	queue->entries[entry].queue_state = COMMAND_NEW;
	//TODO update command data

	pthread_mutex_lock(&(queue->queue_access));

	//insert entry into queue
	if(!queue->head && !queue->tail){
		queue->head = queue->entries + entry;
		queue->tail = queue->head;
	}
	else if(queue->head && queue->tail){
		queue->tail->next = queue->entries + entry;
		queue->entries[entry].prev = queue->tail;
		queue->tail = queue->entries + entry;
	}
	else{
		logprintf(log, LOG_ERROR, "Queue pointers in invalid state\n");
		rv = -1;
	}

	pthread_cond_signal(&(queue->queue_dirty));
	pthread_mutex_unlock(&(queue->queue_access));
	return rv;
}

void commandqueue_free(COMMAND_QUEUE* command_queue){
	size_t i;

	//all dependencies on the mutex and the condition variables should be satisfied,
	//as the worker thread should have been joined prior to calling this function
	pthread_mutex_destroy(&(command_queue->queue_access));
	pthread_cond_destroy(&(command_queue->queue_dirty));

	for(i = 0; i < command_queue->entries_length; i++){
		if(command_queue->entries[i].backing_buffer){
			free(command_queue->entries[i].backing_buffer);
		}
		if(command_queue->entries[i].parameters){
			free(command_queue->entries[i].parameters);
		}
		if(command_queue->entries[i].replies){
			free(command_queue->entries[i].replies);
		}
	}
	if(command_queue->entries){
		free(command_queue->entries);
	}
}
