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

	global $modulelist;
	// init
	$output = Output::getInstance();
	$db = new DB($output);



	// db connection
	if (!$db->connect()) {
		header("HTTP/1.0 500 Database Error!");
		$this->output->write();
		die();
	}

	$http_raw = file_get_contents("php://input");
	$obj = json_decode($http_raw, true);

	$output->addDebugMessage("payload", $obj);

	if (!isset($obj["auth"])) {
		$obj["auth"] = [];
	}

	if (!auth($db, $output, $obj["auth"])) {
		header("WWW-Authenticate: Basic realm=\"cmail Access (Invalid Credentials for " . $_SERVER['PHP_AUTH_USER'] . ")\"");
		header("HTTP/1.0 401 Unauthorized");

		$output->write();
		die();
	}
	if (is_null($module_name)) {

		if (isset($_GET["get_modules"])) {

			$modules = array();
			foreach ($modulelist as $name => $value) {
				$modules[] = $name;
			}

			$output->add("modules", $modules); 
		}
	} else {
		$module = getModuleInstance($module_name, $db, $output);
		foreach ($module->getEndPoints() as $ep => $func) {
			if (isset($_GET[$ep])) {
				$module->$func($obj);
				break;
			}
		}
	}

	$output->write();
}
