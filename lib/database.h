/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

#include <sqlite3.h>
#include <stdbool.h>

#define CMAIL_CURRENT_SCHEMA_VERSION	10
#ifndef SQLITE_DETERMINISTIC
	#define SQLITE_DETERMINISTIC	0x800
#endif
