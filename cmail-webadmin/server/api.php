<?php

/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

include_once("config.php");
include_once("output.php");
include_once("pdo_sqlite.php");
include_once("user.php");
include_once("address.php");

main();

function getApiEndPoint() {
	$api_points = array(
		"get_user",
		"get_users",    
		"add_user",
		"delete_user",
		"add_address",
		"delete_address",
		"update_user",
		"update_address"
	);

	foreach ($api_points as $point) {
		if (isset($_GET[$point])) {
			return $point;
		}
	}
}

function main() {

	// init
	$output = Output::getInstance();
	$db = new DB();

	$user = new User($db, $output);

	// db connection
	if (!$db->connect()) {
		header("WWW-Authenticate: Basic realm=\"Garfield API Access (Invalid Credentials for " . $_SERVER['PHP_AUTH_USER'] . ")\"");
		header("HTTP/1.0 401 Unauthorized");

		die();
	}

	$endPoint = getApiEndPoint();
	$http_raw = file_get_contents("php://input");
	$obj = json_decode($http_raw, true);

	$output->addDebugMessage("payload", $obj);

	//TODO: implement auth

	switch ($endPoint) {
	case "get_users":
		$user->getAll();
		break;
	case "get_user":
		$user->get($obj["username"]);
		break;
	case "add_user":
		$user->add($obj["user"]);
		break;
	case "delete_user":
		$user->delete($obj["username"]);
		break;
	case "add_address":
		break;
	case "delete_address":
		break;
	case "update_user":
		$user->update($obj["user"]);
		break;
	case "update_address":
		break;
	default:
		$output->add("status", "nothing to do.");
		break;
	}

	$output->write();
}