#pragma once

#include "../lib/logger.h"
#include "../lib/common.h"

#include "../lib/common.c"
#include "../lib/logger.c"

// database functions
#include "../lib/database.h"
#include "../lib/database.c"

#ifdef CMAIL_ADMIN_AUTH
// auth functions
#include "../lib/auth.h"
#include "../lib/auth.c"
#endif

#define DEFAULT_DBPATH "/var/cmail/master.db3"
