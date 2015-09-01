.bail on
.echo on

BEGIN TRANSACTION;
	-- update schema version --
	DELETE FROM meta WHERE key='schema_version' AND value='8';
	INSERT OR ROLLBACK INTO meta (key, value) VALUES ('schema_version', '9');

	-- database changes --
	DROP INDEX IF EXISTS idx_addressexpr_per_user;
	
	CREATE TABLE addresses_upd9 (
		address_expression	TEXT	NOT NULL, -- UNIQUE constraint breaks some tricks and should not be needed (ordering is enforced)
		address_order		INTEGER	PRIMARY KEY AUTOINCREMENT NOT NULL UNIQUE,
		address_router		TEXT NOT NULL DEFAULT ('drop'),
		address_route		TEXT
	);

	INSERT INTO addresses_upd9 (address_expression, address_order, address_router, address_route) SELECT address_expression, address_order, COALESCE(msa_inrouter, 'drop'), msa_inroute FROM addresses LEFT JOIN msa ON address_user = msa_user;
	DROP TABLE addresses;
	ALTER TABLE addresses_upd9 RENAME TO addresses;
	CREATE INDEX idx_addr_router ON addresses (address_router, address_route);

	CREATE TABLE smtpd (
		smtpd_user		TEXT PRIMARY KEY NOT NULL REFERENCES users (user_name) ON DELETE CASCADE ON UPDATE CASCADE,
		smtpd_router		TEXT NOT NULL DEFAULT ('drop'),
		smtpd_route		TEXT
	);
	INSERT INTO smtpd (smtpd_user, smtpd_router, smtpd_route) SELECT msa_user, msa_outrouter, msa_outroute FROM msa;
	DROP TABLE msa;

COMMIT;
