//this function _will_ modify the input buffer
/*ssize_t protocol_utf7_decode(char* input){
	size_t decode_pos, b64end, b64len;
	size_t output_pos = 0;

	for(decode_pos = 0; input[decode_pos]; decode_pos++){
		if((input[decode_pos] >= 0x20 && input[decode_pos] <= 0x25)
				|| (input[decode_pos] >= 0x27 && input[decode_pos] <= 0x7e)){
			//literal representation
			input[output_pos++] = input[decode_pos];
		}
		else if(input[decode_pos] == '&'){
			if(input[decode_pos + 1] == '-'){
				//literal ampersand
				input[output_pos++] = '&';
				decode_pos++;
			}
			else{
				//shift into modified base64

				//scan for base64 end and transform to sane base64
				for(b64end = decode_pos + 1; input[b64end]; b64end++){
					if(input[b64end] == '-'){
						break;
					}
					else if(input[b64end] == ','){
						input[b64end] = '/';
					}
				}

				if(!input[b64end]){
					logprintf(LOG_ERROR, "Unterminated base64 in UTF7\n");
					return -1;
				}

				input[b64end] = 0;
				decode_pos++;

				logprintf(LOG_DEBUG, "Base64 to decode: %s\n", input + decode_pos);

				//run through base64 decoder
				b64len = common_base64decode(input + decode_pos, false);
				if(b64len < 0){
					return -1;
				}
				log_dump_buffer(LOG_DEBUG, input + decode_pos, b64len);
				//logprintf(LOG_DEBUG, "Base64 after decode: %s\n", input + decode_pos);

				//copy result back to output pointer and check that no invalid
				//characters were encoded
				for(b64len = 0; input[decode_pos + b64len]; b64len++){
					//if((input[decode_pos + b64len] >= 0x20 && input[decode_pos + b64len] <= 0x25)
					//		|| (input[decode_pos + b64len] >= 0x27 && input[decode_pos + b64len] <= 0x7e)){
					//	logprintf(LOG_ERROR, "UTF-7 contained escaped literal characters (%c @ %d)\n", input[decode_pos + b64len], decode_pos + b64len);
					//	return -1;
					//}
					//else{
						//FIXME this needs to operate on 16-bit characters
						input[output_pos++] = input[decode_pos + b64len];
					//}
				}

				logprintf(LOG_DEBUG, "Decoded %d bytes of base64-encoded UTF-16\n", b64len);
				if(b64len % 2){
					logprintf(LOG_ERROR, "Decoded data has odd length\n");
					return -1;
				}
				decode_pos = b64end + 1;
			}
		}

		//terminate the string
		if(decode_pos > output_pos){
			input[output_pos] = 0;
		}
	}

	return 0;
}*/

//this function may destructively modify the input buffer (in the case of a quoted string)
ssize_t protocol_parse_astring(char* input, char** string_begin, char** data_end, char* illegal_chars){
	//non-ASTRING chars: anything <= 32, (){%*\" 
	char* astring_illegal = illegal_chars ? illegal_chars:"(){%*\\\"";
	size_t length = 0, i;
	char* current_position = NULL;

	if(input[0] == '{' && isdigit(input[1])){
		//literal string
		//RFC 3501 does not specify whether the literal length is guaranteed to be base 10, assuming it anyway
		length = strtoul(input + 1, &current_position, 10);

		if(current_position[0] == '+'){
			current_position++;
		}

		//check the header inserted by the line parser
		if(strncmp(current_position, "}+", 2)){
			//invalid length specified
			return -1;
		}

		//validate string
		for(i = 0; i < length; i++){
			if(current_position[i + 2] == 0){
				//literal can't contain \0 bytes
				return -1;
			}
		}

		*string_begin = current_position + 2;
		if(data_end){
			*data_end = current_position + length + 2;
		}
		return length;
	}
	else if(input[0] == '"'){
		//quoted string
		length = 1;
		for(i = 1; input[i]; i++){
			if(input[i] == '\\'){
				//escaped character, move and copyback
				if(input[i + 1] == '\\' || input[i + 1] == '"'){
					i++;
				}
				else{
					return -1;
				}
			}
			else if(input[i] == '"'){
				//unescaped DQUOTE ends quotedstring -> set string_end
				break;
			}
			//RFC 3501 is unclear on whether quoted-strings may contain \0 characters
			//(CHAR does not resolve in the abnf, assuming it means CHAR8, \0 bytes are not allowed)
			//currently, this is filtered out in the condition, but we're checking it anyway
			//Update: The main note on section 9 states that \0 is illegal in all cases
			else if(input[i] == 0 || input[i] == '\r' || input[i] == '\n'){
				//illegal character in quotedstring
				return -1;
			}

			input[length] = input[i];
			length++;
		}

		//FIXME assert that the end of the string is a DQUOTE
		*string_begin = input + 1;
		if(data_end){
			*data_end = input + i + 1;
		}
		return length - 1;
	}
	else if(input[0] > 32 && !index(astring_illegal, input[0])){
		//sequence of ASTRING-CHARS
		for(length = 0; input[length] > 32 && !index(astring_illegal, input[length]); length++){
		}

		//FIXME if within ASTRING-CHARS sequence and encountering illegal character, return failure

		*string_begin = input;
		if(data_end){
			*data_end = input + length;
		}
		return length;
	}

	//illegal ATOM-CHAR string or invalid quoted string opening
	return -1;
}

ssize_t protocol_next_line(char* buffer, size_t* append_offset_p, ssize_t* new_bytes_p){
	//This function needs to be called on a buffer until it returns 0,
	//otherwise, the append_offset points to the end of the "olddata" buffer

	size_t append_offset = *append_offset_p;
	ssize_t new_bytes = *new_bytes_p;
	int i;

	if(new_bytes < 0){
		logprintf(LOG_ERROR, "protocol_next_line called with error condition in last read, bailing\n");
		return 0;
	}

	logprintf(LOG_DEBUG, "Next line parser called with offset %d bytes %d\n", append_offset, new_bytes);

	//this condition still works within the IMAP literal string syntax because
	//CHAR8 explicitly forbids \0 (RFC 3501 9: CHAR8)
	if(append_offset > 1 && buffer[append_offset - 1] == 0 && buffer[append_offset - 2] == 0){
		//copyback clearing of last line
		for(i = 0; i < new_bytes; i++){
			//logprintf(LOG_DEBUG, "Moving character %02X from position %d to %d\n", client_data->recv_buffer[client_data->recv_offset+i+1+c], client_data->recv_offset+i+1+c, c);
			buffer[i] = buffer[append_offset + i];
		}
		append_offset = 0;

		logprintf(LOG_DEBUG, "Copyback done, offset %d, first byte %02X, %d bytes\n", append_offset, buffer[0], new_bytes);
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

				logprintf(LOG_DEBUG, "Next line parser detected literal string opening, requesting continuation\n");
				return -2;
			}

			//terminate line
			buffer[append_offset + i] = 0;
			buffer[append_offset + i + 1] = 0;

			//send to line processor
			*append_offset_p = append_offset + i + 2; //append after the second \0
			*new_bytes_p = new_bytes - i - 2;

			logprintf(LOG_DEBUG, "Next line contains %d bytes\n", append_offset + i);
			return append_offset + i;
		}
	}

	//no more lines left
	*append_offset_p = append_offset + new_bytes;
	*new_bytes_p = 0;

	logprintf(LOG_DEBUG, "Incomplete line buffer contains %d bytes\n", *append_offset_p);
	return -1;
}

char* protocol_split_parameters(char* inputfmt, char* input, unsigned maxparam, char** parameters){
	unsigned param = 0, offset = 0;
	int astring_length = 0;
	char* data_offset = input;

	for(; inputfmt[offset] && param < maxparam; offset++){
		switch(inputfmt[offset]){
			case 'a':
			case 'l':
				//parse astring / list-mailbox string
				logprintf(LOG_DEBUG, "Parsing %cstring at offset %d: %s\n", inputfmt[offset], data_offset - input, data_offset);
				astring_length = protocol_parse_astring(data_offset, parameters + param, &data_offset, (inputfmt[offset] == 'a') ? NULL:"(){\\\"");
				if(astring_length < 0){
					//failed to parse astring
					logprintf(LOG_ERROR, "Failed to parse astring in command parameter\n");
					return NULL;
				}
				else{
					//terminate parameter
					if(inputfmt[offset + 1] == ' ' || inputfmt[offset + 1] == 0){
						if(*data_offset != ' ' && *data_offset != 0){
							//astring-char sequence contained illegal character, return failure
							//this hack-fixes a similar problem in protocol_parse_astring
							logprintf(LOG_WARNING, "Failing parameter decode because parameters were not space-separated\n");
							return NULL;
						}
						//parameter trailed by SP
						parameters[param][astring_length] = 0;
						data_offset++;
					}
					else{
						//this is a bit harder
						//FIXME terminate parameter strings when not separated with space
						logprintf(LOG_ERROR, "Implementation dead end hit: need to terminate parameters after parsing\n");
						return NULL;
					}
				}
				param++;
				break;
			case ' ':
				//skip this
				break;
			default:
				logprintf(LOG_WARNING, "Unknown parameter format specifier %c\n", inputfmt[offset]);
				break;
		}
	}

	return data_offset;
}
