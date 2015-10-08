/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

void signal_handle(int signum){
	abort_signaled = 1;
}

int signal_init(LOGGER log){
	struct sigaction act = {
		.sa_handler=&signal_handle
	};

	//FIXME use sigaction for this too
	signal(SIGPIPE, SIG_IGN);

	if(sigaction(SIGTERM, &act, NULL) < 0 || sigaction(SIGINT, &act, NULL) < 0) {
		logprintf(log, LOG_ERROR, "Failed to set signal mask\n");
		return -1;
	}

	return 0;
}
