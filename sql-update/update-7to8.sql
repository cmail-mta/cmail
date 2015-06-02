.bail on
.echo on

BEGIN TRANSACTION;

	-- update schema version --
	DELETE FROM meta WHERE key='schema_version' AND value='7';
	INSERT OR ROLLBACK INTO meta (key, value) VALUES ('schema_version', '8');

	-- database changes --
	ALTER TABLE users ADD COLUMN user_alias TEXT REFERENCES users (user_name) ON DELETE SET NULL ON UPDATE CASCADE;
COMMIT;
