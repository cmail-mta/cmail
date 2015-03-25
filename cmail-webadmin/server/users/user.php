<?php

	include_once("../module.php");

	/**
	 * Class for user related things.
	 */
	class User implements Module {

		private $db;
		private $output;
		private $endPoints = array(
			"get" => "get",
			"add" => "add",
			"delete" => "delete",
			"update" => "set_password"
		);

		/**
		 * Constructor for the user class.
		 * @param $db the db object
		 * @param $output the output object
		 */
		public function __construct($db, $output) {
			$this->db = $db;
			$this->output = $output;	
		}

		/**
		 * Returns all endpoints for this module.
		 * @return list of endpoints as keys and function name as values.
		 */
		public function getEndPoints() {
			return $this->endPoints;
		}

		/**
		 * Returns the given user when in database. If no user is defined, send all users.
		 * @param obj object with key username into
		 * @output_flags users all users that matches this username (should be one)
		 * @return list of users that matches (should be one)
		 */
		public function get($obj, $write = true) {

			if (!isset($obj["username"])) {
				// if no username is set, return all users
				return $this->getAll();
			}

			$username = $obj["username"];

			$sql = "SELECT user_name, (user_authdata IS NOT NULL) AS user_login FROM users WHERE user_name = :user_name";
		
			$params = array(":user_name" => $username);

			$out = $this->db->query($sql, $params, DB::F_ARRAY);

			$out["modules"] = $this->getActiveModules($username);

			if ($write) {
				$this->output->add("users", $out);
			}

			return $out;
		}

		public function getActiveModules($username) {
			global $modulelist;
			
			$modules = array();

			foreach ($modulelist as $name => $address) {
				if ($name != "User") {
					$module = getModuleInstance($name, $this->db, $this->output);
					error_log("DEBUG:::" . $name . " = " . json_encode($module));

					if (isset($module)) {
						$modules[$name] = $module->isActive($username);
					}
				}
			}

			return $modules;
		}

		public function isActive($username) {

			$obj = array("username" => $username);

			return (count($this->get($obj, false)) > 0);
		}

		/**
		 * Returns all users in database.
		 * @output_flags users the userlist
		 * @return list of users
		 */
		public function getAll() {

			$sql = "SELECT user_name, (user_authdata IS NOT NULL) AS user_login FROM users";

			$out = $this->db->query($sql, array(), DB::F_ARRAY);
			$this->output->add("users", $out);

			return $out;

		}

		public function create_password_hash($salt, $password) {


			if (is_null($salt)) {
				$salt = uniqid(mt_rand(), true);
				
				$hash = $salt . ":" . hash("sha256", $salt . $password);
				return $hash;
			} else {
				return hash("sha265", $salt . $password);
			}
		}

		/**
		 * Adds a user to database.
		 * @param $user the user object. Every user needs at least a name.
		 *              Valid fields are:
		 *              	* user_name
		 *              	* user_authdata
		 * @output_flags user contains true or false when it failed.
		 * @return @see @output_flags
		 */
		public function add($user) {

			if (!isset($user["user_name"]) || empty($user["user_name"])) {
				$this->output->add("status", "Username is not set.");
				return false;
			}

			if (isset($user["user_authdata"]) && !empty($user["user_authdata"]) && $user["user_authdata"] !== "") {

				$user["user_authdata"] = $this->create_password_hash(null, $user["user_authdata"]);	
				
			} else {
				$user["user_authdata"] = null;
			}


			$sql = "INSERT INTO users(user_name, user_authdata) VALUES (:user_name, :user_authdata)";

			$params = array(
				":user_name" => $user["user_name"],
				":user_authdata" => $user["user_authdata"],
			);

			$id = $this->db->insert($sql, array($params));

			if (isset($id) && !empty($id)) {
				$this->output->add("user", true);
				return true;
			} else {
				$this->output->add("user", false);
				return false;
			}

		}

		/**
		 * Sets the password for the given user
		 * @param $user user object with
		 * 	- user_name name of the user
		 * 	- user_authdata password of the user (if null login is not possible)
		 * @return true or false
		 */
		public function set_password($user) {


			if (!isset($user["user_name"]) || empty($user["user_name"])) {
				$this->output->add("status", "Username is not set.");
				return false;
			}

			if (is_null($user["user_authdata"]) || $user["user_authdata"] === "") {
				$auth = null;
			} else {

				$auth = $this->create_password_hash(null, $user["user_authdata"]);
			}

			$sql = "UPDATE users SET user_authdata = :user_authdata WHERE user_name = :user_name";

			$params = array(
				":user_name" => $user["user_name"],
				":user_authdata" => $auth
			);

			$status = $this->db->insert($sql, array($params));

			if (isset($status)) {
				$this->output->add("status", "ok");
				return true;
			} else {
				return false;
			}
		}

		/**
		 * Deletes a user.
		 * @param $username name of the user
		 * @output_flags delete is "ok" when everything is fine, else "not ok"
		 * @return false on error, else true
		 */
		public function delete($username) {
			//TODO: check input
			$sql = "DELETE FROM users WHERE user_name = :username";

			$params = array(":username" => $username);

			$status = $this->db->insert($sql, array($params));

			$this->output->addDebugMessage("delete", $status);
			if (isset($status)) {
				$this->output->add("delete", "ok");
				return true;
			} else {
				return false;
			}
		}
	}

?>
