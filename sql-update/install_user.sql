.bail on
.echo on

BEGIN TRANSACTION;

	-- create tables --
	CREATE TABLE mailbox (

		mail_id			INTEGER	PRIMARY KEY AUTOINCREMENT
						NOT NULL
						UNIQUE,
		mail_user		TEXT	NOT NULL,
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

	INSERT INTO meta (key, value) VALUES ('schema_version', '8');

COMMIT;
