.bail on
.echo on

BEGIN TRANSACTION;

	-- update schema version --
	DELETE FROM meta WHERE key='schema_version' AND value='3';
	INSERT OR ROLLBACK INTO meta (key, value) VALUES ('schema_version', '4');

	-- database changes --
	ALTER TABLE mailbox ADD COLUMN mail_ident TEXT;
COMMIT;

