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


class Controller {


	private $output;
	private $db;
	private $auth;

	public function __construct($db, $output, $auth) {

		$this->output = $output;
		$this->db = $db;
		$this->auth = $auth;
	}

	public function getOutput() {
		return $this->output;
	}

	public function getDB() {
		return $this->db;
	}

	public function getAuth() {
		return $this->auth;
	}
}


function main($module_name) {

	global $modulelist, $dbpath;
	// init
	if(!session_start()) {
		die();
	}
	$output = Output::getInstance();
	$db = new DB($dbpath, $output);

	$API_VERSION = 6;

	// db connection
	if (!$db->connect()) {
		header("HTTP/1.0 500 Database Error!");
		$output->write();
		die();
	}


	$sql = "SELECT * FROM meta WHERE key = 'schema_version'";

	$out = $db->query($sql, array(), DB::F_SINGLE_ASSOC);

	$output->add("api_version", $API_VERSION);
	$output->add("schema_version", $out["value"]);

	if ($out["value"] != $API_VERSION) {
		$output->add("status", "Api version and schema version is not the same.");
		$output->write();
		die();
	}

	$http_raw = file_get_contents("php://input");
	$obj = json_decode($http_raw, true);

	$output->addDebugMessage("payload", $obj);

	if (!isset($obj["auth"])) {
		$obj["auth"] = [];
	}

	$auth = Auth::getInstance($db, $output);
	$c = new Controller($db, $output, $auth);

	if (!$auth->auth($obj["auth"])) {
		$output->add("login", false);
		header("WWW-Authenticate: Basic realm=\"cmail Access (Invalid Credentials)\"");
		header("HTTP/1.0 401 Unauthorized");

		$output->write();
		die();
	}

	$output->add("login", true);
	$output->add("auth_user", $auth->getUser());

	if (is_null($module_name)) {

		if (isset($_GET["get_modules"])) {

			$modules = array();
			foreach ($modulelist as $name => $value) {
				$modules[] = $name;
			}

			$output->add("modules", $modules);
		}
	} else {
		$module = getModuleInstance($module_name, $c);
		foreach ($module->getEndPoints() as $ep => $func) {
			if (isset($_GET[$ep])) {
				$module->$func($obj);
				break;
			}
		}
	}

	$output->write();
}
