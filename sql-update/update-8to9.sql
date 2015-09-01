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

	INSERT INTO addresses_upd9 (address_expression, address_order, address_router, address_route) SELECT address_expression, address_order, COALESCE(msa_inrouter, 'drop'), CASE msa_inrouter WHEN 'store' THEN msa_user ELSE msa_inroute END FROM addresses LEFT JOIN msa ON address_user = msa_user;
	DROP TABLE addresses;
	ALTER TABLE addresses_upd9 RENAME TO addresses;
	CREATE INDEX idx_addr_router ON addresses (address_router, address_route);
	UPDATE addresses SET address_router = 'redirect' WHERE address_router = 'forward';

	CREATE TABLE smtpd (
		smtpd_user		TEXT PRIMARY KEY NOT NULL REFERENCES users (user_name) ON DELETE CASCADE ON UPDATE CASCADE,
		smtpd_router		TEXT NOT NULL DEFAULT ('drop'),
		smtpd_route		TEXT
	);
	INSERT INTO smtpd (smtpd_user, smtpd_router, smtpd_route) SELECT msa_user, msa_outrouter, msa_outroute FROM msa;
	DROP TABLE msa;

	-- rename column api_right to api_permission in api_access --
	ALTER TABLE api_access RENAME TO api_access_temp;
	CREATE TABLE api_access (
		api_user	TEXT	NOT NULL
					REFERENCES users (user_name)	ON DELETE CASCADE
									ON UPDATE CASCADE,
		api_permission	TEXT	NOT NULL,
		CONSTRAINT api_user_permission UNIQUE (api_user, api_permission) ON CONFLICT FAIL
	);
	INSERT INTO api_access(api_user, api_permission) SELECT api_user, api_right FROM api_access_temp;
	DROP TABLE api_access_temp; 

COMMIT;
