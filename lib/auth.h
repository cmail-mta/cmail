#include <nettle/base16.h>
#include <nettle/sha2.h>

int auth_base64decode(LOGGER log, char* in);
int auth_hash(char* hash, unsigned hash_bytes, char* salt, unsigned salt_bytes, char* pass, unsigned pass_bytes);
#ifdef CMAIL_HAVE_DATABASE_TYPE
int auth_validate(LOGGER log, DATABASE* database, char* user, char* password);
#endif