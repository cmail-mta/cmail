#include <sqlite3.h>
#include <stdbool.h>

#define CMAIL_CURRENT_SCHEMA_VERSION	7
#ifndef SQLITE_DETERMINISTIC
	#define SQLITE_DETERMINISTIC	0x800
#endif
