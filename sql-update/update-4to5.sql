.bail on
.echo on

BEGIN TRANSACTION;

	-- update schema version --
	DELETE FROM meta WHERE key='schema_version' AND value='4';
	INSERT OR ROLLBACK INTO meta (key, value) VALUES ('schema_version', '5');

	-- database changes --
	CREATE TABLE api_access (
		api_user 		TEXT	NOT NULL
						REFERENCES users ( user_name ) 	ON DELETE CASCADE
										ON UPDATE CASCADE,
		api_right		TEXT	NOT NULL,
		CONSTRAINT api_user_right UNIQUE ( api_user, api_right ) ON CONFLICT FAIL
	);

	CREATE TABLE api_address_delegates (
		api_user		TEXT	NOT NULL
						REFERENCES users ( user_name ) 	ON DELETE CASCADE
										ON UPDATE CASCADE,
		api_expression		TEXT	NOT NULL,
		CONSTRAINT api_user_expression UNIQUE ( api_user, api_expression ) ON CONFLICT FAIL
	);

	CREATE TABLE api_user_delegates (
		api_user		TEXT	NOT NULL
						REFERENCES users ( user_name ) 	ON DELETE CASCADE
										ON UPDATE CASCADE,
		api_delegate		TEXT	NOT NULL
						REFERENCES users ( user_name ) 	ON DELETE CASCADE
										ON UPDATE CASCADE,
		CONSTRAINT api_user_delegate UNIQUE ( api_user, api_delegate ) ON CONFLICT FAIL
	);


	-- add admin right to every user (to avoid lock outs) --
	-- IMPORTANT: remove access to non admin users!!!! --
	INSERT INTO api_access (api_user, api_right) SELECT user_name as api_user, 'admin' AS api_right FROM users;

COMMIT;
