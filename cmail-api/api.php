<?php

$API_VERSION = 9;

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

	public function checkSchemaVersion() {
		global $API_VERSION;

		$sql = "SELECT * FROM meta WHERE key = 'schema_version'";

		$out = $this->db->query($sql, array(), DB::F_SINGLE_ASSOC);
		$this->output->add("api_version", $API_VERSION);
		$this->output->add("schema_version", $out["value"]);

		if ($out["value"] != $API_VERSION) {
			return false;
		}

		return true;
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

	// db connection
	if (!$db->connect()) {
		header("HTTP/1.0 500 Database Error!");
		$output->write();
		die();
	}

	$auth = Auth::getInstance($db, $output);
	$c = new Controller($db, $output, $auth);

	if (!$c->checkSchemaVersion()) {
		$output->add("status", "Api version and schema version is not the same.");
		$output->write();
		die();
	}

	// read post
	$http_raw = file_get_contents("php://input");
	$obj = json_decode($http_raw, true);

	$output->addDebugMessage("payload", $obj);

	// check for auth data
	if (!isset($obj["auth"])) {
		$obj["auth"] = [];
	}


	if (!$auth->auth($obj["auth"])) {
		$output->add("login", false);
		$output->write();
		return;
	}

	$output->add("login", true);
	$output->add("auth_user", $auth->getUser());

	// check for modules
	if (is_null($module_name)) {

		if (isset($_GET["get_modules"])) {

			$modules = array();
			foreach ($modulelist as $name => $value) {
				$modules[] = $name;
			}

			$output->add("modules", $modules);
		}
	} else {
		// read modules
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
