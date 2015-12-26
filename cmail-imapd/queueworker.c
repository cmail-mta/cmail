void* queueworker_coreloop(void* param){
	THREAD_CONFIG* thread_config = (THREAD_CONFIG*) param;
	LOGGER log = thread_config->log;
	COMMAND_QUEUE* queue = thread_config->queue;

	logprintf(log, LOG_DEBUG, "Queue worker acquiring mutex...\n");
	pthread_mutex_lock(&(queue->queue_access));

	logprintf(log, LOG_INFO, "Queue worker entering main loop\n");
	while(!abort_signaled){
		logprintf(log, LOG_DEBUG, "Queue worker running queue...\n");
		pthread_cond_wait(&(queue->queue_dirty), &(queue->queue_access));
	}

	pthread_mutex_unlock(&(queue->queue_access));
	return 0;
}
