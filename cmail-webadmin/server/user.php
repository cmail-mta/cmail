<?php

	/**
	 * Class for user related things.
	 */
	class User {

		private $db;
		private $output;

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
		 * Returns the given user when in database.
		 * @param $username the name of the user
		 * @output_flags users all users that matches this username (should be one)
		 * @return list of users that matches (should be one)
		 */
		public function get($username) {

			if (!isset($username)) {
				$this->output->addDebugMessage("user-get","Username must be set.");
				return array();
			}

			$sql = "SELECT user_name, (user_authdata IS NOT NULL) AS user_login, user_inrouter, user_inroute, user_outrouter, user_outroute FROM users WHERE user_name = :user_name";
			$params = array(":user_name" => $username);

			$out = $this->db->query($sql, $params, DB::F_ARRAY);
			$this->output->add("users", $out);

			return $out;
		}

		/**
		 * Returns all users in database.
		 * @output_flags users the userlist
		 * @return userlist
		 */
		public function getAll() {

			$sql = "SELECT user_name, (user_authdata IS NOT NULL) AS user_login, user_inrouter, user_inroute, user_outrouter, user_outroute FROM users";

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
		 *              	* name
		 *              	* authdata
		 *              	* inrouter
		 *              	* inroute
		 *              	* outrouter
		 *              	* outroute
		 *              For valid routers @see routers.
		 * @output_flags user contains the user id or -1 when it failed.
		 * @return @see @output_flags
		 */
		public function add($user) {
			//TODO: check input

			if (!isset($user["user_name"]) || empty($user["user_name"])) {
				$this->output->add("status", "Username is not set.");
				return -2;
			}

			if (!isset($user["user_inrouter"]) || empty($user["user_inrouter"])) {
				$this->output->add("status", "User inrouter is not set.");
				return -3;
			}

			if (!isset($user["user_outrouter"]) || empty($user["user_outrouter"])) {
				$this->output->add("status", "User outrouter is not set.");
				return -4;
			}

			if (isset($user["user_authdata"]) && !empty($user["user_authdata"]) && $user["user_authdata"] !== "") {

				$user["user_authdata"] = $this->create_password_hash(null, $user["user_authdata"]);	
				
			} else {
				$user["user_authdata"] = null;
			}


			$sql = "INSERT INTO users(user_name, user_authdata, user_inrouter, user_inroute, user_outrouter, user_outroute)" 
				. "VALUES (:user_name, :user_authdata, :user_inrouter, :user_inroute, :user_outrouter, :user_outroute)";

			$params = array(
				":user_name" => $user["user_name"],
				":user_authdata" => $user["user_authdata"],
				":user_inrouter" => $user["user_inrouter"],
				":user_inroute" => $user["user_inroute"],
				":user_outrouter" => $user["user_outrouter"],
				":user_outroute" => $user["user_outroute"]
			);

			$id = $this->db->insert($sql, array($params));
			if (isset($id) && !empty($id)) {
				$this->output->add("user", $id);
				return $id;
			} else {
				$this->output->add("user", -1);
				return -1;
			}

		}

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

		/**
		 * Update a user.
		 * @param $user the user object @see add function for more infos.
		 * @output_flags update is "ok" when everything is fine, else "not ok"
		 * @return false on errer, else true
		 */
		public function update($user) {

			//TODO: check input

			$sql = "UPDATE users SET user_authdata = :user_authdata, "
				. "user_inrouter = :user_inrouter, user_inroute = :user_inroute, user_outrouter = :user_outrouter,"
				. "user_outroute = :user_outroute WHERE user_name = :user_name";

			$params = array(
				":user_name" => $user["user_name"],
				":user_authdata" => $user["user_authdata"],
				":user_inrouter" => $user["user_inrouter"],
				":user_inroute" => $user["user_inroute"],
				":user_outrouter" => $user["user_outrouter"],
				":user_outroute" => $user["user_outroute"]
			);

			$status = $this->db->insert($sql, array($params));

			if (isset($status)) {
				$this->output->add("status", "ok");
				return true;
			} else {
				return false;
			}
		}

	}

?>
