cmail modules

cmail-msa	Mail Submission Agent, accepts incoming mail
cmail-mta	Mail Transfer Agent, delivers outgoing mail
cmail-imapd	IMAP server, provides access to local mailboxes
cmail-popd	POP3 server, provides access to local mailboxes
cmail-webadmin	Web-based administration panel

master database structure

	'users' table
	-------------
	Contains elementary user and routing data, but no mapping
	to mail adresses.

		user_name	TEXT NOT NULL
		Textual user identifier, not directly related to
		any mailbox name. Should be used as user identifier
		in all authentication scenarios (e.g. IMAP auth, 
		POP auth, SMTP auth)

		user_authdata	TEXT
		Representation of the shared secret used in
		authentication scenarios. Current format
		"salt:sha256(salt+password)" where + represents
		the concatenation operator.

		user_database	TEXT
		Primary incoming mail storage database for the user.
		If this database fails, an attempt will be made to
		store to the master database instead.
		
	'addresses' table
	-----------------
	Contains the mapping of addresses (or rather, mail paths)
	to users.

		address_expression	TEXT NOT NULL UNIQUE
		SQLite regular expression defining the path to be mapped.
		
		address_order		INTEGER NOT NULL UNIQUE
		Integer allowing total ordering of addresses for matching.
		If multiple addresses match a given path, the one with the
		highest index "wins".

		address_user		TEXT NOT NULL
		Foreign key into the 'users' table.

	'msa' table
	-----------
	Contains parameters relevant to the mail submission agent,
	responsible for accepting incoming mail and allowing users
	to originate new messages. Users that do not have a router
	definition will be handled with the 'reject' routers and
	addresses mapped to them will be ignored at address resolution.

		msa_user	TEXT NOT NULL PRIMARY KEY
		Foreign key into the 'users' table

		msa_inrouter	TEXT NOT NULL DEFAULT 'store'
		msa_inroute	TEXT
		Mail routing function and parameter used for incoming 
		mail to any address belonging to this user.

		msa_outrouter	TEXT NOT NULL DEFAULT 'drop'
		msa_outroute
		Mail routing function and parameter used for mail 
		submitted by a connection authenticated as this user.

	Valid inbound mail routing functions are as follows:

		Router: store
		Parameter: None
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

	Valid outbound mail routing functions are as follows:

		Router: any
		Parameter: None
		Mail submission is accepted with any sender address in the
		envelope section (relay operation).

		Router: defined
		Parameter: None
		Originating mail must have an adress mapped to the sending 
		user as envelope sender.

		Router: handoff
		Parameter: Mail server address (NULL behaves the same 
			way as the drop router)
		Hand off all originated mail for this user to another server 
		(smarthost) for relaying.

		Router: alias
		Parameter: locally defined user (NULL behaves the same
			way as the drop router)
		Route as the aliased user.

		Router: reject
		Parameter: None
		Reject originated messages for this user and any addresses routed 
		to him.

		Router: drop
		Parameter: None
		Accept, but quietly drop originated messages for this user.

	'mailbox' table
	-------------
	Contains mail for local users.

		mail_id 		INTEGER PRIMARY KEY NOT NULL
		Local mail identifier. No guarantee of temporal uniqueness
		is given.

		mail_user		TEXT NOT NULL
		Foreign key into the 'users' table.

		mail_read		BOOLEAN NOT NULL DEFAULT FALSE
		Flag indicating if the mail was already read.
		Only applicable in access protocols where continuous
		storage is an option.

		mail_envelopeto		TEXT NOT NULL
		Envelope recipient address.
	
		mail_envelopefrom	TEXT NOT NULL
		Envelope origin address.

		mail_submission		TEXT NOT NULL
		Unix timestamp of submission.

		mail_submitter		TEXT
		Name or address of submitting peer.

		mail_proto		TEXT NOT NULL DEFAULT 'smtp'
		Protocol the mail was submitted with.

		mail_data		BLOB
		Raw mail data.

	'outbox' table
	--------------
		TBD

	'popd' table
	------------
		TBD

	'meta' table
	------------
		TBD
