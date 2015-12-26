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
			//TODO create local copy of command contents
			pthread_mutex_unlock(&(queue->queue_access));

			//TODO do the actual work

			pthread_mutex_lock(&(queue->queue_access));
			entries_done++;
			//TODO copy back responses
			head->queue_state = COMMAND_REPLY;

			while(head && head->queue_state != COMMAND_NEW){
				head = head->next;
			}
		}

		logprintf(log, LOG_DEBUG, "Queue run handled %d items\n", entries_done);
		entries_done = 0;
		pthread_cond_wait(&(queue->queue_dirty), &(queue->queue_access));
	}

	pthread_mutex_unlock(&(queue->queue_access));
	return 0;
}
