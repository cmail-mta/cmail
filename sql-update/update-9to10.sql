.bail on
.echo on

BEGIN TRANSACTION;
	-- update schema version --
	DELETE FROM meta WHERE key='schema_version' AND value='9';
	INSERT OR ROLLBACK INTO meta (key, value) VALUES ('schema_version', '10');

	-- database changes --
	CREATE TABLE mailbox_names (
		mailbox_id		INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL UNIQUE CHECK(mailbox_id >= 0),
		mailbox_name		TEXT NOT NULL,
		mailbox_parent		INTEGER REFERENCES mailbox_names (mailbox_id) ON DELETE CASCADE ON UPDATE CASCADE
	);

	CREATE TABLE mailbox_mapping (
		mail_id			INTEGER NOT NULL REFERENCES mailbox (mail_id) ON DELETE CASCADE ON UPDATE CASCADE,
		mailbox_id		INTEGER NOT NULL REFERENCES mailbox_names (mailbox_id) ON DELETE CASCADE ON UPDATE CASCADE,
		CONSTRAINT map_unique UNIQUE (mail_id, mailbox_id) ON CONFLICT IGNORE
	);

	INSERT INTO mailbox_names (mailbox_id, mailbox_name) VALUES (0, 'INBOX');
COMMIT;
