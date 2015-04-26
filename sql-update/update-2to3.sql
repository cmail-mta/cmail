.bail on
.echo on

BEGIN TRANSACTION;

	-- update schema version --
	DELETE FROM meta WHERE key='schema_version' AND value='2';
	INSERT OR ROLLBACK INTO meta (key, value) VALUES ('schema_version', '3');

	-- database changes --
	ALTER TABLE users ADD COLUMN user_database TEXT;
COMMIT;
