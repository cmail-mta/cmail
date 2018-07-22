/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

char* common_strdup(char* input){
	char* duplicate = NULL;
	size_t length = strlen(input);
	duplicate = calloc(length + 1, sizeof(char));
	if(!duplicate){
		return NULL;
	}

	return strncpy(duplicate, input, length); //this is ok because the memory is calloc'd
}

int common_rand(void* target, size_t bytes){
	size_t data_read = 0;
	FILE* pool = fopen(RANDOMNESS_POOL, "r");

	if(!pool){
		return -1;
	}

	data_read = fread(target, 1, bytes, pool);

	fclose(pool);
	return data_read;
}

int common_strrepl(char* buffer, unsigned length, char* variable, char* replacement){
	char* occurence = NULL;
	unsigned offset = 0;
	unsigned character_offset;
	unsigned i;

	for(occurence = strstr(buffer + offset, variable); occurence; occurence = strstr(buffer + offset, variable)){
		//check whether the replacement actually fits in here
		if((occurence - buffer) + strlen(replacement) + strlen(buffer + strlen(variable)) >= length){
			return -1;
		}

		//move the trailing part out of the way
		if(strlen(replacement) > strlen(variable)){
			character_offset = strlen(replacement) - strlen(variable);

			//begin at end
			for(i = 0; i <= strlen(occurence); i++){
				occurence[strlen(occurence) - i + character_offset] = occurence[strlen(occurence) - i];
			}
		}
		else if(strlen(replacement) < strlen(variable)){
			character_offset = strlen(variable) - strlen(replacement);

			//begin in front
			for(i = 0; i <= strlen(occurence + strlen(variable)); i++){
				occurence[strlen(replacement) + i] = occurence[strlen(replacement) + i + character_offset];
			}
		}

		//insert the replacement
		strncpy(occurence, replacement, strlen(replacement));

		//increase the offset
		offset = (occurence - buffer) + strlen(replacement);
	}

	return 0;
}

char* common_strappf(char* target, size_t* target_allocated, char* fmt, ...){
	va_list args, copy;

	size_t target_len = target ? strlen(target):0;
	int additional_length = 0;

	va_start(args, fmt);

	va_copy(copy, args);
	additional_length = vsnprintf(NULL, 0, fmt, copy);
	va_end(copy);

	if(additional_length < 0){
		va_end(args);
		return NULL;
	}

	if(target_len + additional_length >= *target_allocated){
		//reallocate
		target = realloc(target, (target_len + additional_length + 1) * sizeof(char));
		if(!target){
			va_end(args);
			return NULL;
		}

		*target_allocated = target_len + additional_length + 1;
	}

	vsnprintf(target + target_len, additional_length + 1, fmt, args);

	va_end(args);
	return target;
}

int common_tprintf(char* format, time_t time, char* buffer, size_t buffer_length){
	struct tm* local_time = localtime(&time);
	if(!local_time){
		return -1;
	}

	if(strftime(buffer, buffer_length, format, local_time) == 0){
		//buffer too short
		return -2;
	}

	return 0;
}

ssize_t common_read_file(char* filename, uint8_t** out){
	uint8_t* buffer = NULL;
	FILE* handle = NULL;
	long file_size = 0;

	*out = NULL;

	handle = fopen(filename, "rb");
	if(!handle){
		return -1;
	}

	fseek(handle, 0, SEEK_END);
	file_size = ftell(handle);
	rewind(handle);

	if(file_size < 0){
		fclose(handle);
		return -1;
	}

	buffer = calloc(file_size + 2, 1);
	if(!buffer){
		fclose(handle);
		return -1;
	}

	if(fread(buffer, file_size, 1, handle) != 1){
		free(buffer);
		fclose(handle);
		return -1;
	}

	fclose(handle);

	*out = buffer;
	return file_size;
}

ssize_t common_next_line(char* buffer, size_t* append_offset_p, ssize_t* new_bytes_p){
	//This function needs to be called on a buffer until it returns 0,
	//otherwise, the append_offset points to the end of the "olddata" buffer
	size_t append_offset = *append_offset_p;
	ssize_t new_bytes = *new_bytes_p;
	int i;

	if(new_bytes < 0){
		logprintf(LOG_ERROR, "common_next_line called with error condition in last read, bailing\n");
		return 0;
	}

	logprintf(LOG_DEBUG, "Next line parser called with offset %d bytes %d\n", append_offset, new_bytes);

	if(append_offset > 1 && buffer[append_offset - 1] == 0 && buffer[append_offset - 2] == 0){
		//copyback clearing of last line

		for(i = 0; i < new_bytes; i++){
			//logprintf(log, LOG_DEBUG, "Moving character %02X from position %d to %d\n", client_data->recv_buffer[client_data->recv_offset+i+1+c], client_data->recv_offset+i+1+c, c);
			buffer[i] = buffer[append_offset + i];
		}

		append_offset = 0;

		logprintf(LOG_DEBUG, "Copyback done, offset %d, first byte %02X, %d bytes\n", append_offset, buffer[0], new_bytes);
	}

	//scan new bytes for terminators
	for(i = 0; i < new_bytes - 1; i++){ //last byte is checked in condition
		if(buffer[append_offset + i] == '\r' && buffer[append_offset + i + 1] == '\n'){
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

/*
call sequence

//CHECK LEFTBYTES

do {
	line_length=next(log, buffer, recv_off, bytes);
	if(length<0){
		//procfail
	}
	else if(length>0){
		if(length>PROTOMAX){
			fail
		}
		else{
			handle
		}
	}
}
while(length>0);


*/
