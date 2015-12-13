/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

ssize_t client_send_raw(LOGGER log, CONNECTION* client, char* data, ssize_t bytes){
	ssize_t bytes_sent = 0, bytes_written = 0, bytes_left;

	//early bail saves some syscalls
	if(bytes == 0){
		return 0;
	}

	if(bytes < 0){
		bytes=strlen(data);
	}

	logprintf(log, LOG_DEBUG, "Sending %d raw bytes\n", bytes);

	do{
		bytes_left = bytes - bytes_sent;
		if(bytes_left > MAX_SEND_CHUNK){
			bytes_left = MAX_SEND_CHUNK;
		}
		#ifndef CMAIL_NO_TLS
		switch(client->tls_mode){
			case TLS_NONE:
				bytes_written = send(client->fd, data + bytes_sent, bytes_left, 0);
				break;
			case TLS_NEGOTIATE:
				logprintf(log, LOG_WARNING, "Not sending data while negotiation is in progress\n");
				return bytes_sent;
			case TLS_ONLY:
				bytes_written = gnutls_record_send(client->tls_session, data + bytes_sent, bytes_left);
				break;
		}
		#else
		bytes_written = send(client->fd, data + bytes_sent, bytes_left, 0);
		#endif

		if(bytes_written + bytes_sent < bytes){
			logprintf(log, LOG_DEBUG, "Partial write (%d for %d/%d)\n", bytes_written, bytes_sent, bytes);
		}

		if(bytes_written < 0){
			#ifndef CMAIL_NO_TLS
			if(client->tls_mode == TLS_NONE){
			#endif
			if(errno != EAGAIN){
				logprintf(log, LOG_ERROR, "Write failed: %s\n", strerror(errno));
				break;
			}
			#ifndef CMAIL_NO_TLS
			}
			else{
				if(bytes_written != GNUTLS_E_INTERRUPTED && bytes_written != GNUTLS_E_AGAIN){
					logprintf(log, LOG_ERROR, "TLS Write failed: %s\n", gnutls_strerror(bytes_written));
					break;
				}
			}
			#endif
		}
		else{
			bytes_sent += bytes_written;
		}
	}
	while(bytes_sent < bytes);

	logprintf(log, LOG_ALL_IO, "<< %.*s", bytes, data);
	logprintf(log, LOG_DEBUG, "Sent %d bytes of %d\n", bytes_sent, bytes);

	return bytes_sent;
}

int client_send(LOGGER log, CONNECTION* client, char* fmt, ...){
	va_list args, copy;
	ssize_t bytes = 0;
	char static_send_buffer[STATIC_SEND_BUFFER_LENGTH + 1];
	char* dynamic_send_buffer = NULL;
	char* send_buffer = static_send_buffer;

	va_start(args, fmt);
	va_copy(copy, args);
	//check if the buffer was long enough, if not, allocate a new one
	bytes = vsnprintf(send_buffer, STATIC_SEND_BUFFER_LENGTH, fmt, args);
	va_end(args);

	if(bytes >= STATIC_SEND_BUFFER_LENGTH){
		dynamic_send_buffer = calloc(bytes + 2, sizeof(char));
		if(!dynamic_send_buffer){
			logprintf(log, LOG_ERROR, "Failed to allocate dynamic send buffer\n");
			va_end(copy);
			return -1;
		}
		send_buffer = dynamic_send_buffer;
		bytes = vsnprintf(send_buffer, bytes + 1, fmt, copy);
	}
	va_end(copy);

	if(bytes < 0){
		logprintf(log, LOG_ERROR, "Failed to render client output data string\n");
		return -1;
	}

	bytes = client_send_raw(log, client, send_buffer, bytes);

	if(dynamic_send_buffer){
		free(dynamic_send_buffer);
	}

	return bytes;
}
