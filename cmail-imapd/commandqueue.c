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
		
		.tag_offset = 0,
		.command_offset = 0,
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
			empty.parameters[i] = -1;
		}
	}

	*entry = empty;
	return;
}

int commandqueue_initialize(LOGGER log, COMMAND_QUEUE* command_queue){
	size_t i;

	//allocate initial queue entry buffer
	command_queue->entries_length = CMAIL_IMAP_QUEUE_MAX;
	command_queue->entries = calloc(CMAIL_IMAP_QUEUE_MAX, sizeof(QUEUED_COMMAND));
	for(i = 0; i < command_queue->entries_length; i++){
		commandqueue_reset_entry(command_queue->entries + i, false);
	}
	return 0;
}

int commandqueue_enqueue(LOGGER log, COMMAND_QUEUE* queue, IMAP_COMMAND command, char** parameters){
	size_t entry;
	int rv = 0;
	unsigned i;

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
		logprintf(log, LOG_WARNING, "Command queue exhausted\n");
		//pthread_mutex_lock(&(queue->queue_access));
		//this can only happen if no entry is currently IN_PROGRESS in order to avoid dangling pointers
		//this also needs to update all next/previous/head/tail pointers
		//reallocate entries array
		//FIXME this can actually never happen, as we can only do this when the queue workers head pointer is not in use
		//as there is no way to guarantee this, this is not possible
		//since this does not happen anymore and the entry memory can thus be assumed to be static,
		//the queue worker will not create local copies of the entries. when allowing this feature again in the future,
		//that needs to be taken into account
		//pthread_mutex_unlock(&(queue->queue_access));

		//bail out
		return -1;
	}

	//reset the entry
	commandqueue_reset_entry(queue->entries + entry, true);

	//store command data
	//FIXME this allocation strategy might not be optimal and should probably be replaced
	//by a zero-copy storage design passing around complete buffers.
	//FIXME this could also be optimized by not relying on command.backing_buffer_length as command length
	if(queue->entries[entry].backing_buffer_length < command.backing_buffer_length){
		queue->entries[entry].backing_buffer = realloc(queue->entries[entry].backing_buffer, command.backing_buffer_length * sizeof(char));
		if(!queue->entries[entry].backing_buffer){
			logprintf(log, LOG_ERROR, "Failed to allocate memory for queue entry backing buffer\n");
			return -1;
		}
		queue->entries[entry].backing_buffer_length = command.backing_buffer_length;
	}
	memcpy(queue->entries[entry].backing_buffer, command.backing_buffer, command.backing_buffer_length);

	//copy tag / command info
	queue->entries[entry].tag_offset = command.tag - command.backing_buffer;
	queue->entries[entry].command_offset = command.command - command.backing_buffer;

	//fix up parameter pointers
	//this requires that all parameter pointers are offsets into command.backing_buffer, but that should always be the case...
	if(parameters){
		//count how many there are in the first place
		for(i = 0; parameters[i]; i++){
		}

		if(i + 1 > queue->entries[entry].parameters_length){
			//reallocate parameters array
			queue->entries[entry].parameters = realloc(queue->entries[entry].parameters, (i + 1) * sizeof(int));
			if(!queue->entries[entry].parameters){
				logprintf(log, LOG_ERROR, "Failed to allocate memory for command queue entry parameter array\n");
				return -1;
			}
			queue->entries[entry].parameters_length = i + 1;
		}

		for(i = 0; parameters[i]; i++){
			queue->entries[entry].parameters[i] = parameters[i] - command.backing_buffer;
			queue->entries[entry].parameters[i + 1] = -1;
		}
	}

	//update bookkeeping data
	queue->entries[entry].active = true;
	queue->entries[entry].last_enqueue = time(NULL);
	queue->entries[entry].queue_state = COMMAND_NEW;

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
