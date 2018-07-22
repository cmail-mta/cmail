/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#define MAX_CFGLINE 			2048
#define STATIC_SEND_BUFFER_LENGTH	1024
#define RANDOMNESS_POOL 		"/dev/urandom"

char* common_strdup(char* input);
int common_rand(void* target, size_t bytes);
int common_strrepl(char* buffer, size_t length, char* variable, char* replacement);
char* common_strappf(char* target, size_t* target_allocated, char* fmt, ...);
int common_tprintf(char* format, time_t time, char* buffer, size_t buffer_length);
ssize_t common_read_file(char* filename, uint8_t** out);
ssize_t common_next_line(char* buffer, size_t* append_offset_p, ssize_t* new_bytes_p);
