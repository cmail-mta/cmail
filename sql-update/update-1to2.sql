BEGIN TRANSACTION;
ALTER TABLE mailbox ADD COLUMN mail_read BOOLEAN NOT NULL DEFAULT(0);
DELETE OR ROLLBACK FROM meta WHERE key='schema_version' AND value='1';
INSERT OR ROLLBACK meta (key, value) VALUES ('schema_version','2');
COMMIT;

