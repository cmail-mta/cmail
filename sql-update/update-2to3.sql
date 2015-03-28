BEGIN TRANSACTION;
ALTER TABLE users ADD COLUMN user_database TEXT;
DELETE OR ROLLBACK FROM meta WHERE key='schema_version' AND value='2';
INSERT OR ROLLBACK meta (key, value) VALUES ('schema_version', '3');
COMMIT;

