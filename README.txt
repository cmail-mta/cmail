cmail modules

cmail-msa	Mail Submission Agent, accepts incoming mail
cmail-mta	Mail Transfer Agent, delivers outgoing mail
cmail-imapd	IMAP server, provides access to local mailboxes

master database structure

	'users' table
	-------------
	Contains elementary user and routing data, but no mapping
	to mail adresses.

		user_name	TEXT NOT NULL
		Textual user identifier, not directly related to
		any mailbox name. Should be in for authentication
		scenarios (e.g. IMAP auth, SMTP auth)

		user_inrouter	TEXT NOT NULL DEFAULT 'store'
		user_inroute	TEXT
		Mail routing function and parameter used for incoming 
		mail to any address belonging to this user.

		user_outrouter	TEXT NOT NULL DEFAULT 'drop'
		user_outroute
		Mail routing function and parameter used for outgoing
		mail from this user.

	Valid inbound mail routing functions are as follows:

		Router: store
		Parameter: target database path (NULL defaults to 
			master database)
		Store the incoming messages in the 'mails' table of
		a database. Local system users may want to have their
		mail delivered to a database in their home directories.
	
		Router: forward
		Parameter: address (NULL behaves the same way as
			the drop router)
		Forward all incoming mail to a specified address.
		Useful for mail aliases to other systems.

		Router: handoff
		Parameter: Mail server address (NULL behaves the same
			way as the drop router)
		Relays the incoming mail to another server. Useful
		for fallback mail server configurations or relay
		applications.

		Router: alias
		Parameter: locally defined user (NULL behaves the same
			way as the drop router)
		Assign all incoming mail to the aliased user, using
		that users configured routing. Useful for shared
		access to a mailbox.

		Router: drop
		Parameter: None
		Accept incoming mail, but do not store it. Might be
		useful.

		Router: reject
		Parameter: None
		Reject incoming mail for this user (default behaviour for
		unknown adresses). Might be useful for send-only setups.

	Valid outbound mail routing functions are as follows 
		(Any outbound use require authentication):

		Router: any
		Parameter: None
		Outgoing mail is accepted with any sender address in the
		envelope section (relay operation).

		Router: defined
		Parameter: None
		Outgoing mail must have an adress mapped to the sending 
		user as envelope sender.

		Router: handoff
		Parameter: Mail server address (NULL behaves the same 
			way as the drop router)
		Hand off all outbound mail for this user to another server 
		(smarthost) for relaying.

		Router: alias
		Parameter: locally defined user (NULL behaves the same
			way as the drop router)
		Route as the aliased user.

		Router: none
		Parameter: None
		Reject outbound messages for this user.

		Router: drop
		Parameter: None
		Accept, but do not send outbound messages for this user.
