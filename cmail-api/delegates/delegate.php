<?php

	include_once("../module.php");

	/**
	 * Class for delegate related things.
	 */
	class Delegate implements Module {

		private $db;
		private $output;
		private $c;
		private $auth;

		// List of end points. Format is:
		// $name => $func
		// $name name of the end point
		// $func name of the function that is called. Function must be in this class.
		private $endPoints = array(
			"get" => "get",
			"user_add" => "addUser",
			"user_delete" => "deleteUser",
			"address_add" => "addAddress",
			"address_delete" => "deleteAddress"
		);

		/**
		 * Constructor for the user class.
		 * @param $db the db object
		 * @param $output the output object
		 */
		public function __construct($c) {
			$this->c = $c;
			$this->db = $c->getDB();
			$this->output = $c->getOutput();
			$this->auth = $c->getAuth();
		}

		/**
		 * Returns all endpoints for this module.
		 * @return list of endpoints as keys and function name as values.
		 */
		public function getEndPoints() {
			return $this->endPoints;
		}


		/**
		 * Returns all delegated users and all delegated addresses of the user
		 * @param $write (optional) if true give delegates to the output module
		 * @return object with
		 * 	users list of delegated users
		 * 	addresses list of delegated addresses
		 */
		public function get($write = true) {

			if ($this->auth->hasRight("admin")) {
				$sql_user = "SELECT * FROM api_user_delegates";
				$sql_addresses = "SELECT * FROM api_address_delegates";
				$params = array();
			} else if ($this->auth->hasRight("delegate")) {
				$sql_user = "SELECT * FROM api_user_delegates WHERE api_user = :api_user";
				$sql_addresses = "SELECT * FROM api_address_delegates WHERE api_user = :api_user";
				$params = array(
					":api_user" => $this->auth->getUser()
				);
			} else {
				$sql_user = "SELECT * FROM api_user_delegates WHERE api_delegate = :api_user";
				$sql_addresses = "SELECT api_user, api_expression FROM api_address_delegates 
					JOIN addresses ON (address_expression = api_expression) WHERE address_user = :api_user";
				$params = array(
					":api_user" => $this->auth->getUser()
				);
			}

			$delegates = array(
				"users" => $this->db->query($sql_user, $params, DB::F_ARRAY),
				"addresses" => $this->db->query($sql_addresses, $params, DB::F_ARRAY)
			);

			if ($write) {
				$this->output->add("delegates", $delegates);
			}

			return $delegates;
		}

		/**
		 * Return if the module is active for the user
		 * @param username
		 * @return true or false
		 */
		public function isActive($username) {

			if (!isset($username) || empty($username)) {
				return false;
			}

			$sql = "SELECT :api_user AS user, 
				(SELECT count(*) FROM api_user_delegates WHERE api_delegate = :api_user) AS users,
				(SELECT count(*) FROM api_address_delegates 
				JOIN addresses ON (api_expression LIKE address_expression) WHERE address_user = :api_user) AS addresses";

			$params = array(
				":api_user" => $username
			);

			$out = $this->db->query($sql, $params, DB::F_ARRAY);

			if (count($out) < 1) {
				return false;
			}

			$this->output->addDebugMessage("delegates", $out);
			return $out[0]["users"] + $out[0]["addresses"] > 0;
		}

		public function addUser($obj, $delegated = false) {
			
			if (!isset($obj["api_user"]) || empty($obj["api_user"])) {
				$this->output->add("status", "User is not set.");
				return false;
			}
			
			if (!isset($obj["api_delegate"]) || empty($obj["api_delegate"])) {
				$this->output->add("status", "Delegated user is not set.");
				return false;
			}
			$auth = Auth::getInstance($this->db, $this->output);

			if (!$auth->hasRight("admin") && !$delegated) {
				return false;
			}

			$sql = "INSERT INTO api_user_delegates (api_user, api_delegate) VALUES (:api_user, :api_delegate)";

			$params = array(
				":api_user" => $obj["api_user"],
				":api_delegate" => $obj["api_delegate"]
			);

			return $this->db->insert($sql, [$params]);
		}

		public function deleteUser($obj) {
			if (!isset($obj["api_user"]) || empty($obj["api_user"])) {
				$this->output->add("status", "User is not set.");
				return false;
			}
			
			if (!isset($obj["api_delegate"]) || empty($obj["api_delegate"])) {
				$this->output->add("status", "Delegated user is not set.");
				return false;
			}
			$auth = Auth::getInstance($this->db, $this->output);

			if (!$auth->hasRight("admin")) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "DELETE FROM api_user_delegates WHERE api_user = :api_user AND api_delegate = :api_delegate";

			$params = array(
				":api_user" => $obj["api_user"],
				":api_delegate" => $obj["api_delegate"]
			);

			return $this->db->insert($sql, [$params]);

		}

		public function addAddress($obj) {
			$auth = Auth::getInstance($this->db, $this->output);

			if (!isset($obj["api_user"]) || empty($obj["api_user"])) {
				$this->output->add("status", "User is not set.");
				return false;
			}
			if (!isset($obj["api_expression"]) || empty($obj["api_expression"])) {
				$this->output->add("status", "Address expression is not set.");
				return false;
			}
			if (!$auth->hasRight("admin")) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "INSERT INTO api_address_delegates (api_user, api_expression) VALUES (:api_user, :api_expression)";

			$params = array(
				":api_user" => $obj["api_user"],
				":api_expression" => $obj["api_expression"]
			);

			return $this->db->insert($sql, [$params]);
		}

		public function deleteAddress($obj, $write = true) {
			$auth = Auth::getInstance($this->db, $this->output);

			if (!isset($obj["api_user"]) || empty($obj["api_user"])) {
				$this->output->add("status", "User is not set.");
				return false;
			}
			if (!isset($obj["api_expression"]) || empty($obj["api_expression"])) {
				$this->output->add("status", "Address expression is not set.");
				return false;
			}
			if (!$auth->hasRight("admin")) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "DELETE FROM api_address_delegates WHERE api_user = :api_user AND api_expression = :api_expression";

			$params = array(
				":api_user" => $obj["api_user"],
				":api_expression" => $obj["api_expression"]
			);

			return $this->db->insert($sql, [$params]);
		}

	}
?>
