<?php
	$dbpath = "/var/db/cmail.db3";

	// list of modules
	// key => value
	// key = class name
	// value = path to php file
	$modulelist = array(
		"Address" => "addresses/address.php",
		"User" => "users/user.php",
		"MSA" => "msa/msa.php",
		"POP" => "pop/pop.php",
		"Mailbox" => "mailbox/mailbox.php"
	);

?>
