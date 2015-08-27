.bail on
.echo on

BEGIN TRANSACTION;
	CREATE TABLE users (
		user_name	TEXT NOT NULL UNIQUE,
		user_authdata	TEXT,
		user_database	TEXT,
		user_alias 	TEXT REFERENCES users (user_name) ON DELETE SET NULL ON UPDATE CASCADE
	);

	CREATE TABLE addresses (
		address_expression	TEXT	NOT NULL, -- UNIQUE constraint breaks some tricks and should not be needed (ordering is enforced)
		address_order		INTEGER	PRIMARY KEY AUTOINCREMENT NOT NULL UNIQUE,
		address_router		TEXT NOT NULL DEFAULT ('drop'),
		address_route		TEXT
	);

	-- This index ensures that cmail-admin-address parameters exactly match one address entry
	-- With update 9 this became redundant because address_user was replaced and address_order is already
	-- indexed as primary key
	CREATE UNIQUE INDEX idx_addr_ident ON addresses (
		address_expression,
		address_order
	);

	CREATE INDEX idx_addr_router ON addresses (
		address_router,
		address_route
	);

	CREATE TABLE mailbox (

		mail_id			INTEGER	PRIMARY KEY AUTOINCREMENT
						NOT NULL
						UNIQUE,
		mail_user		TEXT	NOT NULL
						REFERENCES users ( user_name )	ON DELETE CASCADE
										ON UPDATE CASCADE,
		mail_read		BOOLEAN	NOT NULL
						DEFAULT ( 0 ),
		mail_ident		TEXT,
		mail_envelopeto		TEXT	NOT NULL,
		mail_envelopefrom	TEXT	NOT NULL,
		mail_submission		TEXT	NOT NULL
						DEFAULT ( strftime( '%s', 'now' ) ),
		mail_submitter		TEXT,
		mail_proto		TEXT	NOT NULL
						DEFAULT ( 'smtp' ),
		mail_data		BLOB
	);

	CREATE TABLE meta (
		[key]	TEXT	PRIMARY KEY
				NOT NULL
				UNIQUE,
		value	TEXT
	);

	CREATE TABLE smtpd (
		smtpd_user		TEXT PRIMARY KEY NOT NULL REFERENCES users (user_name) ON DELETE CASCADE ON UPDATE CASCADE,
		smtpd_router		TEXT NOT NULL DEFAULT ('drop'),
		smtpd_route		TEXT
	);

	CREATE TABLE outbox (
		mail_id			INTEGER PRIMARY KEY AUTOINCREMENT
						NOT NULL
						UNIQUE,
		mail_remote		TEXT,
		mail_envelopefrom	TEXT,
		mail_envelopeto		TEXT	NOT NULL,
		mail_submission		INTEGER	NOT NULL
						DEFAULT ( strftime ( '%s', 'now' ) ),
		mail_submitter		TEXT,
		mail_data		TEXT
	);

	CREATE TABLE popd (
		pop_user 		TEXT	PRIMARY KEY
						NOT NULL
						UNIQUE
						REFERENCES users ( user_name )	ON DELETE CASCADE
										ON UPDATE CASCADE,
		pop_lock		BOOLEAN NOT NULL
						DEFAULT ( 0 )
	);

	CREATE TABLE api_access (
		api_user 		TEXT	NOT NULL
						REFERENCES users ( user_name ) 	ON DELETE CASCADE
										ON UPDATE CASCADE,
		api_right		TEXT	NOT NULL,
		CONSTRAINT api_user_right UNIQUE ( api_user, api_right ) ON CONFLICT FAIL
	);

	CREATE TABLE api_address_delegates (
		api_user		TEXT	NOT NULL
						REFERENCES users ( user_name ) 	ON DELETE CASCADE
										ON UPDATE CASCADE,
		api_expression		TEXT	NOT NULL,
		CONSTRAINT api_user_expression UNIQUE ( api_user, api_expression ) ON CONFLICT FAIL
	);

	CREATE TABLE api_user_delegates (
		api_user		TEXT	NOT NULL
						REFERENCES users ( user_name ) 	ON DELETE CASCADE
										ON UPDATE CASCADE,
		api_delegate		TEXT	NOT NULL
						REFERENCES users ( user_name ) 	ON DELETE CASCADE
										ON UPDATE CASCADE,
		CONSTRAINT api_user_delegate UNIQUE ( api_user, api_delegate ) ON CONFLICT FAIL
	);

	CREATE TABLE faillog (
		fail_mail	INTEGER NOT NULL REFERENCES outbox (mail_id) ON DELETE CASCADE ON UPDATE CASCADE,
		fail_message	TEXT,
		fail_time	TEXT NOT NULL DEFAULT (strftime( '%s', 'now' )),
		fail_fatal	BOOLEAN NOT NULL DEFAULT (0)
	);

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

	INSERT INTO meta (key, value) VALUES ('schema_version', '9');
COMMIT;
