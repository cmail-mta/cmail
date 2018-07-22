/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

#include <nettle/base16.h>
#include <nettle/sha2.h>

int auth_base64decode(char* in);
int auth_hash(char* hash, unsigned hash_bytes, char* salt, unsigned salt_bytes, char* pass, unsigned pass_bytes);
int auth_validate(sqlite3_stmt* auth_data, char* user, char* password, char** authorized_identity);
