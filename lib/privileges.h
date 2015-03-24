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
