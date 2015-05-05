.bail on
.echo on

BEGIN TRANSACTION;

	-- update schema version --
	DELETE FROM meta WHERE key='schema_version' AND value='5';
	INSERT OR ROLLBACK INTO meta (key, value) VALUES ('schema_version', '6');

	-- database changes --
	CREATE TABLE faillog (
		fail_mail	INTEGER NOT NULL REFERENCES outbox (mail_id) ON DELETE CASCADE ON UPDATE CASCADE,
		fail_message	TEXT,
		fail_time	TEXT NOT NULL DEFAULT (strftime( '%s', 'now' )),
		fail_fatal	BOOLEAN NOT NULL DEFAULT (0)
	);


COMMIT;
