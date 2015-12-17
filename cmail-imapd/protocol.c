int protocol_parse_astring(){
	return 0;
}

ssize_t protocol_next_line(LOGGER log, char* buffer, size_t* append_offset_p, ssize_t* new_bytes_p){
	//This function needs to be called on a buffer until it returns 0,
	//otherwise, the append_offset points to the end of the "olddata" buffer
	
	size_t append_offset = *append_offset_p;
	ssize_t new_bytes = *new_bytes_p;
	int i;

	if(new_bytes < 0){
		logprintf(log, LOG_ERROR, "protocol_next_line called with error condition in last read, bailing\n");
		return 0;
	}

	logprintf(log, LOG_DEBUG, "Next line parser called with offset %d bytes %d\n", append_offset, new_bytes);

	//this condition still works within the IMAP literal string syntax because
	//CHAR8 explicitly forbids \0 (RFC 3501 9: CHAR8)
	if(append_offset > 1 && buffer[append_offset - 1] == 0 && buffer[append_offset - 2] == 0){
		//copyback clearing of last line
		for(i = 0; i < new_bytes; i++){
			//logprintf(log, LOG_DEBUG, "Moving character %02X from position %d to %d\n", client_data->recv_buffer[client_data->recv_offset+i+1+c], client_data->recv_offset+i+1+c, c);
			buffer[i] = buffer[append_offset + i];
		}
		append_offset = 0;
		
		logprintf(log, LOG_DEBUG, "Copyback done, offset %d, first byte %02X, %d bytes\n", append_offset, buffer[0], new_bytes);
	}

	//scan new bytes for terminators
	for(i = 0; i < new_bytes - 1; i++){ //last byte is checked in condition
		if(buffer[append_offset + i] == '\r' && buffer[append_offset + i + 1] == '\n'){
			//test for no more new bytes after this and close-brace indicating string literal
			if(i == new_bytes - 2 && append_offset + i > 0 && buffer[append_offset + i - 1] == '}'){
				//TODO test this with an email containing a {foo} line
				//the main conflict point would probably APPEND, which is probably
				//only ever sent in a literal string - as that is the only format capable of
				//transferring CRLF segments
				
				//if this condition is met when an astring is expected, send a continuation request
				//and subsequently read N bytes

				//replace line terminator with marker
				buffer[append_offset + i] = '+';
				buffer[append_offset + i + 1] = 0;

				*append_offset_p = append_offset + i + 1;
				*new_bytes_p = new_bytes - i - 2; //should always be 0
				
				logprintf(log, LOG_DEBUG, "Next line parser detected literal string opening, requesting continuation\n");
				return -2;
			}
			
			//terminate line
			buffer[append_offset + i] = 0;
			buffer[append_offset + i + 1] = 0;

			//send to line processor
			*append_offset_p = append_offset + i + 2; //append after the second \0
			*new_bytes_p = new_bytes - i - 2;

			logprintf(log, LOG_DEBUG, "Next line contains %d bytes\n", append_offset + i);
			return append_offset + i;
		}
	}

	//no more lines left
	*append_offset_p = append_offset + new_bytes;
	*new_bytes_p = 0;

	logprintf(log, LOG_DEBUG, "Incomplete line buffer contains %d bytes\n", *append_offset_p);
	return -1;
}
