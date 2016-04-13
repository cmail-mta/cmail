.bail on
.echo on

BEGIN TRANSACTION;
	-- update schema version --
	DELETE FROM meta WHERE key='schema_version' AND value='10';
	INSERT OR ROLLBACK INTO meta (key, value) VALUES ('schema_version', '11');

	-- database changes --
	CREATE TABLE flags (
		mail_id		INTEGER NOT NULL REFERENCES mailbox (mail_id) ON DELETE CASCADE ON UPDATE CASCADE,
		flag TEXT	NOT NULL,
		CONSTRAINT flag_unique_per_mail UNIQUE (mail_id, flag) ON CONFLICT FAIL
	);
COMMIT;
