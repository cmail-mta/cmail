.bail on
.echo on

BEGIN TRANSACTION;

	-- The users table stores information on cmail users
	--		user_name		User handle
	--		user_authdata		Password salt and hash in the format
	--					<salt>:<sha256(salt + password)>
	--		user_database		Path to a user database if on is to be loaded
	--		user_alias		If this account should be an alias, this is the account it is aliased to
	CREATE TABLE users (
		user_name	TEXT NOT NULL UNIQUE,
		user_authdata	TEXT,
		user_database	TEXT,
		user_alias 	TEXT REFERENCES users (user_name) ON DELETE SET NULL ON UPDATE CASCADE
	);

	-- The addresses table stores inbound path information
	-- It is also used in the `defined` outbound router
	--		address_expression	The path expression
	--		address_order		Numerical order/weight value, see the documentation
	--		address_router		The inbound router to be used for the path
	--		address_route		An (optional) argument to the router

	CREATE TABLE addresses (
		address_expression	TEXT NOT NULL, -- UNIQUE constraint breaks some tricks and should not be needed (ordering is enforced)
		address_order		INTEGER	PRIMARY KEY AUTOINCREMENT NOT NULL UNIQUE,
		address_router		TEXT NOT NULL DEFAULT ('drop'),
		address_route		TEXT
	);

	-- We frequently query routers for address expressions, so we better index this
	CREATE INDEX idx_addr_router ON addresses (
		address_router,
		address_route
	);

	-- The mailbox table is the primary mail storage in the master database
	--		mail_id			A unique identifier for any mail
	--		mail_user		The user the mail is stored to
	--		mail_read		A boolean specifying whether the mail was already read 
	--					(currently only used by the webmailer)
	--					This field may be removed in the future and should not be
	--					relied on
	--		mail_ident		A textual identifier for a given mail
	--		mail_envelopeto		The path given in the RCPT TO section of the SMTP dialog
	--		mail_envelopefrom	The path given in the MAIL FROM section of the SMTP dialog
	--		mail_submission		Timestamp of reception
	--		mail_submitter		DNS reverse name of submitting host
	--		mail_proto		Protocol the mail was submitted with
	--		mail_data		The actual content
	CREATE TABLE mailbox (
		mail_id			INTEGER	PRIMARY KEY AUTOINCREMENT NOT NULL UNIQUE,
		mail_user		TEXT NOT NULL REFERENCES users (user_name) ON DELETE CASCADE ON UPDATE CASCADE,
		mail_read		BOOLEAN	NOT NULL DEFAULT (0),
		mail_ident		TEXT,
		mail_envelopeto		TEXT NOT NULL,
		mail_envelopefrom	TEXT NOT NULL,
		mail_submission		TEXT NOT NULL DEFAULT (strftime('%s', 'now')),
		mail_submitter		TEXT,
		mail_proto		TEXT NOT NULL DEFAULT ('smtp'),
		mail_data		BLOB
	);

	-- This table contains meta-information used by the system to check e.g. database schema compliance
	CREATE TABLE meta (
		[key]			TEXT PRIMARY KEY NOT NULL UNIQUE,
		value			TEXT
	);

	-- The smtpd table contains information on whether a user may authenticate with the smtp daemon in order to
	-- originate mails, and what outbound router to use in that case
	--		smtpd_user		The user to be allowed to authenticate
	--		smtpd_router		The outbound router to apply
	--		smtpd_route		An (optional) router argument
	CREATE TABLE smtpd (
		smtpd_user		TEXT PRIMARY KEY NOT NULL REFERENCES users (user_name) ON DELETE CASCADE ON UPDATE CASCADE,
		smtpd_router		TEXT NOT NULL DEFAULT ('drop'),
		smtpd_route		TEXT
	);

	-- The outbox table contains all mail queued to be sent out from the server, such as originated mail and bounce notifications.
	-- It needs to be cross-referenced with faillog to yield information as to the state of the queued mail.
	--		mail_id			A unique identifier for originated mail
	--		mail_remote		NULL if delivery is made via resolving MX records of the envelope destination, an MX to connect otherwise
	--		mail_envelopefrom	Envelope sender
	--		mail_envelopeto		Envelope destination
	--		mail_submission		Timestamp of entry into the delivery queue
	--		mail_submitter		Originating user
	--		mail_data		Mail contents
	CREATE TABLE outbox (
		mail_id			INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL UNIQUE,
		mail_remote		TEXT,
		mail_envelopefrom	TEXT,
		mail_envelopeto		TEXT NOT NULL,
		mail_submission		INTEGER	NOT NULL DEFAULT (strftime ('%s', 'now')),
		mail_submitter		TEXT,
		mail_data		TEXT
	);

	-- The POP3 spec requires that only one connection to a maildrop (which maps to one user in our model) be open at any time.
	-- The pop daemon sets the pop_lock flag to 1 when a connection is active.
	--		pop_user	POP maildrop user name
	--		pop_lock	Flag indicating lock status
	CREATE TABLE popd (
		pop_user		TEXT PRIMARY KEY NOT NULL UNIQUE REFERENCES users (user_name) ON DELETE CASCADE ON UPDATE CASCADE,
		pop_lock		BOOLEAN NOT NULL DEFAULT (0)
	);

	CREATE TABLE api_access (
		api_user 		TEXT NOT NULL REFERENCES users (user_name) ON DELETE CASCADE ON UPDATE CASCADE,
		api_permission		TEXT NOT NULL,
		CONSTRAINT api_user_permission UNIQUE ( api_user, api_permission ) ON CONFLICT FAIL
	);

	CREATE TABLE api_address_delegates (
		api_user		TEXT NOT NULL REFERENCES users (user_name) ON DELETE CASCADE ON UPDATE CASCADE,
		api_expression		TEXT NOT NULL,
		CONSTRAINT api_user_expression UNIQUE ( api_user, api_expression ) ON CONFLICT FAIL
	);

	CREATE TABLE api_user_delegates (
		api_user		TEXT NOT NULL REFERENCES users (user_name) ON DELETE CASCADE ON UPDATE CASCADE,
		api_delegate		TEXT NOT NULL REFERENCES users (user_name) ON DELETE CASCADE ON UPDATE CASCADE,
		CONSTRAINT api_user_delegate UNIQUE ( api_user, api_delegate ) ON CONFLICT FAIL
	);

	-- The faillog table contains delivery attempt information for mails in the outbox table.
	-- Entries are written by the dispatch daemon when attempts at delivery are made.
	--		fail_mail	Outbound mail queue identifier
	--		fail_message	(Optional) Server response for failed attempt
	--		fail_time	Timestamp of delivery attempt
	--		fail_fatal	Flag indicating whether this attempt returned a permanent failure code
	CREATE TABLE faillog (
		fail_mail		INTEGER NOT NULL REFERENCES outbox (mail_id) ON DELETE CASCADE ON UPDATE CASCADE,
		fail_message		TEXT,
		fail_time		TEXT NOT NULL DEFAULT (strftime('%s', 'now')),
		fail_fatal		BOOLEAN NOT NULL DEFAULT (0)
	);

	-- This view correlates the outbox and faillog tables.
	-- The dispatch daemon will periodically scan this view for mail eligible for delivery (ie, not yet delivered or previously failed temporarily),
	-- attempt to deliver it and, upon failure, either generate bounce notifications or insert a temporary failure entry into faillog
	--		mail_id			See outbox
	--		mail_remote		See outbox
	--		mail_envelopefrom	See outbox
	--		mail_envelopeto		See outbox
	--		mail_submission		See outbox
	--		mail_submitter		See outbox
	--		mail_data		See outbox
	--		mail_failcount		Number of failed delivery attempts
	--		mail_lasttry		Timestamp of last delivery attempt
	--		mail_fatality		Flag indicating whether any delivery attempts returned permanent failure codes
	CREATE VIEW outbound AS
	SELECT
		mail_id,
		mail_remote,
		mail_envelopefrom,
		mail_envelopeto,
		mail_submission,
		mail_submitter,
		mail_data,
		COUNT( fail_mail ) AS mail_failcount,
		COALESCE( MAX( fail_time ) , 0 ) AS mail_lasttry,
		COALESCE( SUM( fail_fatal ) , 0 ) AS mail_fatality
	FROM outbox
	LEFT JOIN faillog ON mail_id = fail_mail
	GROUP BY mail_id;

	-- Store the current schema version to the database
	INSERT INTO meta (key, value) VALUES ('schema_version', '9');
COMMIT;
