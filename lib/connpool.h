/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

typedef struct /*_CONNECTION_AGGREGATE*/ {
	unsigned count;
	CONNECTION* conns;
} CONNPOOL;
