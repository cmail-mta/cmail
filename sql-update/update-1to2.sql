BEGIN TRANSACTION;
ALTER TABLE mailbox ADD COLUMN mail_read BOOLEAN NOT NULL DEFAULT(0);
DELETE FROM meta WHERE key='schema_version' AND value='1';
INSERT OR ROLLBACK INTO meta (key, value) VALUES ('schema_version','2');
COMMIT;

