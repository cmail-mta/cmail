int queueworker_arbitrate_command(LOGGER log, QUEUED_COMMAND* entry){
	unsigned i;

	logprintf(log, LOG_DEBUG, "Handling command [%s]: %s\n", entry->tag, entry->command);
	if(entry->parameters){
		for(i = 0; entry->parameters[i] > 0; i++){
			logprintf(log, LOG_DEBUG, "Command parameter %d: %s\n", i, entry->backing_buffer + entry->parameters[i]);
		}
	}

	if(!strcasecmp(entry->command, "noop")){
		//TODO check for new mail
		return NULL;
	}
	else if(!strcasecmp(entry->command, "xyzzy")){
		//round-trip xyzzy
		entry->replies = common_strappf(entry->replies, &(entry->replies_length),
				"* XYZZY Round-trip completed\r\n%s OK Incantation completed\r\n", entry->tag);
		if(!entry->replies){
			//FIXME this is really bad and probably should be adressed by falling back to a static failure response buffer
			logprintf(log, LOG_ERROR, "Failed to reallocate command reply\n");
		}
		return 0;
	}

	//FIXME need to be able to increase failscore here
	return -1;
}

void* queueworker_coreloop(void* param){
	THREAD_CONFIG* thread_config = (THREAD_CONFIG*) param;
	LOGGER log = thread_config->log;
	COMMAND_QUEUE* queue = thread_config->queue;
	QUEUED_COMMAND* head = NULL;
	unsigned entries_done = 0;

	logprintf(log, LOG_DEBUG, "Queue worker acquiring mutex\n");
	pthread_mutex_lock(&(queue->queue_access));

	logprintf(log, LOG_INFO, "Queue worker entering main loop\n");
	while(!abort_signaled){
		logprintf(log, LOG_DEBUG, "Queue worker running queue\n");

		if(queue->head){
			head = queue->head;
			while(head && head->queue_state != COMMAND_NEW){
				head = head->next;
			}
		}

		while(head){
			//process queue head
			head->queue_state = COMMAND_IN_PROGRESS;
			pthread_mutex_unlock(&(queue->queue_access));

			//TODO do the actual work
			if(queueworker_arbitrate_command(log, head) < 0){
				logprintf(log, LOG_WARNING, "Command execution returned error in queue worker\n");
			}


			entries_done++;
			pthread_mutex_lock(&(queue->queue_access));
			head->queue_state = COMMAND_REPLY;

			while(head && head->queue_state != COMMAND_NEW){
				//canceled commands need to round-trip through the worker because the head pointer is thread-local
				if(head->queue_state == COMMAND_CANCELED){
					head->queue_state = COMMAND_CANCEL_ACK;
				}
				head = head->next;
			}
		}

		logprintf(log, LOG_DEBUG, "Queue run handled %d items\n", entries_done);
		if(entries_done > 0){
			//notify feedback pipe
			//FIXME might want to check the return value here
			write(thread_config->feedback_pipe[1], "c", 1);
		}
		entries_done = 0;
		pthread_cond_wait(&(queue->queue_dirty), &(queue->queue_access));
	}

	logprintf(log, LOG_DEBUG, "Queue worker shutting down\n");
	pthread_mutex_unlock(&(queue->queue_access));
	return 0;
}
