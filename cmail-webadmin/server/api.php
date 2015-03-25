<?php

/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

require_once("config.php");
require_once("output.php");
require_once("pdo_sqlite.php");

// auth plugin
require_once("auth.php");

function main($module_name) {

	// init
	$output = Output::getInstance();
	$db = new DB($output);


	$module = new $module_name($db, $output);

	// db connection
	if (!$db->connect()) {
		header("WWW-Authenticate: Basic realm=\"cmail API Access (Invalid Credentials for " . $_SERVER['PHP_AUTH_USER'] . ")\"");
		header("HTTP/1.0 401 Unauthorized");

		die();
	}

	$http_raw = file_get_contents("php://input");
	$obj = json_decode($http_raw, true);

	$output->addDebugMessage("payload", $obj);

	if (!isset($obj["auth"])) {
		$obj["auth"] = null;
	}	

	if (!auth($db, $output, $obj["auth"])) {
		header("WWW-Authenticate: Basic realm=\"cmail Access (Invalid Credentials for " . $_SERVER['PHP_AUTH_USER'] . ")\"");
		header("HTTP/1.0 401 Unauthorized");

		$output->write();
		die();
	}


	foreach ($module->getEndPoints() as $ep => $func) {
		if (isset($_GET[$ep])) {
			$module->$func($obj);
			break;
		}
	}

	$output->write();
}
