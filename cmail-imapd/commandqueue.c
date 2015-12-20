int commandqueue_initialize(LOGGER log, COMMAND_QUEUE* command_queue){
	//TODO
	return -1;
}

int commandqueue_enqueue(LOGGER log, IMAP_COMMAND* command, char** parameters){
	//TODO
	return -1;
}

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

void commandqueue_free(COMMAND_QUEUE* command_queue){
	size_t i;

	//TODO assert all conditional waits on the mutex are satisfied
	pthread_mutex_destroy(&(command_queue->queue_access));

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
