<?php

	/**
	 * authorization function
	 * @param db database connection
	 * @param output output object
	 * @param auth object with authorization data (like user, pw)
	 * @return true if ok, else false
	 */
	function auth($db, $output, $auth) {
		$auth = array();
		if (!isset($_SERVER["PHP_AUTH_USER"]) || empty($_SERVER["PHP_AUTH_USER"])) {
			return false;
		}

		if (!isset($_SERVER["PHP_AUTH_PW"]) || empty($_SERVER["PHP_AUTH_PW"])) {
			return false;
		}

		$auth["user_name"] = $_SERVER["PHP_AUTH_USER"];
		$auth["password"] = $_SERVER["PHP_AUTH_PW"];

		if (!isset($auth["user_name"]) || empty($auth["user_name"]) ) {
			$output->add("status", "No auth username set.");
			return false;
		}

		$sql = "SELECT user_authdata FROM users WHERE user_name = :user_name";

		$params = array(
			":user_name" => $auth["user_name"]
		);

		$user = $db->query($sql, $params, DB::F_ARRAY);


		if (count($user) < 1) {
			$output->add("status", "Username not found.");
			return false;	
		}

		$user = $user[0];

		if (!isset($user["user_authdata"])) {
			$output->add("status", "User is not allowed to login.");
			return false;
		}

		$password = explode(":", $user["user_authdata"]);

		$pass = hash("sha256", $password[0] . $auth["password"]);

		return ($password[1] === $pass);
	}

?>
