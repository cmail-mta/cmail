<?php
	include_once("../module.php");

	class SMTPD implements Module {


		private $db;
		private $output;
		private $c;
		// possible endpoints
		private $endPoints = array(
			"get" => "get",
			"add" => "add",
			"update" => "update",
			"delete" => "delete"
		);

		public function __construct($c) {
			$this->c = $c;
			$this->db = $c->getDB();
			$this->output = $c->getOutput();
		}

		/**
		 * Return all end points with functions
		 * @return list of end points with (endpoint = key and function name as value)
		 */
		public function getEndPoints() {
			return $this->endPoints;
		}


		public function getActiveUsers() {

			$sql = "SELECT smtpd_user FROM smtpd GROUP BY smtpd_user";

			$users = $this->c->getDB()->query($sql, array(), DB::F_ARRAY);

			$output = array();
			foreach($users as $user) {
				$output[$user["smtpd_user"]] = true;
			}

			return $output;
		}

		/**
		 * Return all smtpd entries for delegated users and the entry for himself
		 * @param $write if true send list to output module
		 * @return list of users
		 */
		private function getDelegated($write = true) {

			$auth = Auth::getInstance($this->db, $this->output);
			$sql = "SELECT * FROM smtpd WHERE smtpd_user = :api_user OR smtpd_user IN
				(SELECT api_delegate FROM api_user_delegates WHERE api_user = :api_user)";

			$params = array(
				":api_user" => $auth->getUser()
			);

			$out = $this->db->query($sql, $params, DB::F_ARRAY);

			if ($write) {
				$this->output->add("smtpd", $out);
			}

			return $out;
		}

		/**
		 * Returns the smtpd entry for the given username
		 * @param $obj object with
		 * 	- smtpd_user name of the user
		 * @return list of entries with given username (should be one or no entry)
		 */
		public function get($obj, $write = true) {
			if (!isset($obj["smtpd_user"]) || empty($obj["smtpd_user"])) {
				// if no username is set, return all users
				return $this->getAll($write);
			}

			return $this->getByUser($obj["smtpd_user"], $write);
		}

		private function getByUser($username, $write = true) {

			$auth = Auth::getInstance($this->db, $this->output);
			if (!$auth->hasDelegatedUser($username)) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "SELECT * FROM smtpd WHERE smtpd_user = :smtpd_user";

			$params = array(":smtpd_user" => $username);

			$out = $this->db->query($sql, $params, DB::F_ARRAY);
			if ($write) {
				$this->output->add("smtpd", $out);
			}

			return $out;
		}


		public function isActive($username) {

			$obj = array("smtpd_user" => $username);

			return (count($this->get($obj, false)) > 0);
		}

		/**
		 * Returns all smtpd entries
		 * @return list of all smtpd entries in database
		 */
		public function getAll($write = true) {

			$auth = Auth::getInstance($this->db, $this->output);

			if (!$auth->hasPermission("delegate") && !$auth->hasPermission("admin")) {
				return $this->getByUser($auth->getUser());
			}

			if ($auth->hasPermission("delegate") && !$auth->hasPermission("admin")) {
				return $this->getDelegated();
			}

			$sql = "SELECT * FROM smtpd";

			$out = $this->db->query($sql, array(), DB::F_ARRAY);
			if ($write) {
				$this->output->add("smtpd", $out);
			}

			return $out;
		}


		/**
		 * Adds an entry to the smtpd table
		 * @param object with
		 * 	- smtpd_user (required) the username
		 * 	- smtpd_router (required) the outrouter @see routers in docu
		 * 	- smtpd_route (optional) parameter for outrouter
		 * @return true or false (only one entry for one user)
		 */
		public function add($smtpd) {

			if (!isset($smtpd["smtpd_user"]) || empty($smtpd["smtpd_user"])) {
				$this->output->add("status", "Username is not set.");
				return false;
			}
			
			if (!isset($smtpd["smtpd_router"]) || empty($smtpd["smtpd_router"])) {
				$this->output->add("status", "User outrouter is not set.");
				return false;
			}

			$auth = Auth::getInstance($this->db, $this->output);

			if (!$auth->hasDelegatedUser($smtpd["smtpd_user"])) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "INSERT INTO smtpd(smtpd_user, smtpd_router, smtpd_route)"
				. "VALUES (:smtpd_user, :smtpd_router, :smtpd_route)";

			$params = array(
				":smtpd_user" => $smtpd["smtpd_user"],
				":smtpd_router" => $smtpd["smtpd_inrouter"],
				":smtpd_route" => $smtpd["smtpd_inroute"],
			);

			$id = $this->db->insert($sql, array($params));

			if (isset($id) && !empty($id)) {
				$this->output->add("smtpd", $id);
				return true;
			} else {
				$this->output->add("smtpd", -1);
				return false;
			}
		}

		/**
		 * Updates an smtpd entry
		 * @param object with
		 * 	- smtpd_user (required) username
		 * 	- smtpd_router (required) the outrouter @see routers in docu
		 * 	- smtpd_route (optional) the parameter for outrouter
		 * @return true or false
		 */
		public function update($obj) {

			if (!isset($obj["smtpd_user"]) || empty($obj["smtpd_user"])) {
				$this->output->add("status", "Username is not set.");
				return false;
			}


			if (!isset($obj["smtpd_router"]) || empty($obj["smtpd_router"])) {
				$this->output->add("status", "User router is not set.");
				return false;
			}

			$auth = Auth::getInstance($this->db, $this->output);
			if (!$auth->hasDelegatedUser($obj["smtpd_user"])) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "UPDATE smtpd SET smtpd_router = :smtpd_router,"
					. " smtpd_route = :smtpd_route"
					. " WHERE smtpd_user = :smtpd_user";
			$params = array(
				":smtpd_user" => $obj["smtpd_user"],
				":smtpd_router" => $obj["smtpd_router"],
				":smtpd_route" => $obj["smtpd_route"],
			);

			$status = $this->db->insert($sql, array($params));

			return isset($status);
		}


		/**
		 * Delete an entry from smtpd table
		 * @param object with
		 * 	- smtpd_user username
		 * @return true or false
		 */
		public function delete($obj) {

			if (!isset($obj["smtpd_user"]) || empty($obj["smtpd_user"])) {
				$this->output->add("status", "Username is not set.");
				return false;
			}

			$auth = Auth::getInstance($this->db, $this->output);
			if (!$auth->hasDelegatedUser($obj["smtpd_user"])) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "DELETE FROM smtpd WHERE smtpd_user = :smtpd_user";

			$params = array(
				"smtpd_user" => $obj["smtpd_user"]
			);

			$status = $this->db->insert($sql, array($params));

			if (isset($status)) {
				return true;
			}

			return false;
		}
	}

?>
