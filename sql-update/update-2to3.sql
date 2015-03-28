BEGIN TRANSACTION;
ALTER TABLE users ADD COLUMN user_database TEXT;
UPDATE OR ROLLBACK meta SET value='3' WHERE key='schema_version' AND value='2';
COMMIT;

