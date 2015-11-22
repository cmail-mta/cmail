/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

typedef struct /*_PRIVILEGE_COMPOSITE*/ {
	int uid;
	int gid;
} USER_PRIVS;
