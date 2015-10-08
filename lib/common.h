/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#define MAX_CFGLINE 			2048
#define STATIC_SEND_BUFFER_LENGTH	1024
#define RANDOMNESS_POOL 		"/dev/urandom"
