#
# Regular cron jobs for the cmail-mta package
#
0 4	* * *	root	[ -x /usr/bin/cmail-mta_maintenance ] && /usr/bin/cmail-mta_maintenance
