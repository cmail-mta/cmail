.bail on
.echo on

BEGIN TRANSACTION;

	-- update schema version --
	DELETE FROM meta WHERE key='schema_version' AND value='6';
	INSERT OR ROLLBACK INTO meta (key, value) VALUES ('schema_version', '7');

	-- database changes --
	CREATE VIEW outbound AS	
		SELECT 
			mail_id,
			mail_remote,
			mail_envelopefrom,
			mail_envelopeto,
			mail_submission,
			mail_submitter,
			mail_data,
			count( fail_mail ) AS mail_failcount,
			coalesce( max( fail_time ) , 0 ) AS mail_lasttry,
			coalesce( sum( fail_fatal ) , 0 ) AS mail_fatality
			FROM outbox
			LEFT JOIN faillog
			ON mail_id = fail_mail
			GROUP BY mail_id;

COMMIT;
