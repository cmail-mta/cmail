<?php
	include_once("../module.php");

	class MSA implements Module {


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

		/**
		 * Return all msa entries for delegated users and the entry for himself
		 * @param $write if true send list to output module
		 * @return list of users
		 */
		private function getDelegated($write = true) {

			$auth = Auth::getInstance($this->db, $this->output);
			$sql = "SELECT * FROM msa WHERE msa_user = :api_user OR msa_user IN
				(SELECT api_delegate FROM api_user_delegates WHERE api_user = :api_user)";

			$params = array(
				":api_user" => $auth->getUser()
			);

			$out = $this->db->query($sql, $params, DB::F_ARRAY);

			if ($write) {
				$this->output->add("msa", $out);
			}

			return $out;
		}

		/**
		 * Returns the msa entry for the given username
		 * @param $obj object with
		 * 	- msa_user name of the user
		 * @return list of entries with given username (should be one or no entry)
		 */
		public function get($obj, $write = true) {
			if (!isset($obj["msa_user"]) || empty($obj["msa_user"])) {
				// if no username is set, return all users
				return $this->getAll($write);
			}

			return $this->getByUser($obj["msa_user"], $write);
		}

		private function getByUser($username, $write = true) {

			$auth = Auth::getInstance($this->db, $this->output);
			if (!$auth->hasDelegatedUser($username)) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "SELECT * FROM msa WHERE msa_user = :msa_user";

			$params = array(":msa_user" => $username);

			$out = $this->db->query($sql, $params, DB::F_ARRAY);
			if ($write) {
				$this->output->add("msa", $out);
			}

			return $out;
		}


		public function isActive($username) {

			$obj = array("msa_user" => $username);
			
			return (count($this->get($obj, false)) > 0);
		}

		/**
		 * Returns all msa entries
		 * @return list of all msa entries in database 
		 */
		public function getAll($write = true) {

			$auth = Auth::getInstance($this->db, $this->output);

			if (!$auth->hasRight("delegate") && !$auth->hasRight("admin")) {
				return $this->getByUser($auth->getUser());
			}

			if ($auth->hasRight("delegate") && !$auth->hasRight("admin")) {
				return $this->getDelegated();
			}

			$sql = "SELECT * FROM msa";

			$out = $this->db->query($sql, array(), DB::F_ARRAY);
			if ($write) {
				$this->output->add("msa", $out);
			}

			return $out;
		}


		public function transfer($in, $out) {

			//TODO: transfer	
		}

		public function updateUserDatabase($msa, $transfer = false) {

			if (!isset($msa["msa_user"]) || empty($msa["msa_user"])) {
				return false;
			}

			if ($transfer) {
				$msa_old = $this->get($msa, false);

				if ($msa_old["msa_inrouter"] == "store") {
					$this->transfer($msa_old["msa_inroute"], $msa["msa_inroute"]);
				}
			}

			$sql = "UPDATE users SET user_database = :user_database WHERE user_name = :user_name";

			$path = $msa["msa_inroute"];
			if ($msa["msa_inroute"] == "") {
				$path = null;
			}

			$params = array(
				":user_database" => $msa["msa_inroute"],
				":user_name" => $path
			);
			
			$status = $this->db->insert($sql, [$params]);

			return isset($status);

		}

		/**
		 * Adds an entry to the msa table
		 * @param object with
		 * 	- msa_user (required) the username
		 * 	- msa_inrouter (required) the inrouter @see routers in docu
		 * 	- msa_inroute (optional) parameter for inrouter
		 * 	- msa_outrouter (required) the outrouter @see routers in docu
		 * 	- msa_outroute (optional) parameter for outrouter
		 * @return true or false (only one entry for one user)
		 */
		public function add($msa) {

                        if (!isset($msa["msa_user"]) || empty($msa["msa_user"])) {
                                $this->output->add("status", "Username is not set.");
                                return false;
                        }

                        if (!isset($msa["msa_inrouter"]) || empty($msa["msa_inrouter"])) {
                                $this->output->add("status", "User inrouter is not set.");
                                return false;
                        }

                        if (!isset($msa["msa_outrouter"]) || empty($msa["msa_outrouter"])) {
                                $this->output->add("status", "User outrouter is not set.");
                                return false;
			}

			$auth = Auth::getInstance($this->db, $this->output);

			if (!$auth->hasDelegatedUser($msa["msa_user"])) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			if (isset($msa["msa_inroute"]) && !empty($msa["msa_inroute"]) && $msa["msa_inrouter"] == "store") {

				if(!$this->updateUserDatabase($msa)) {
					return false;
				}
			}

                        $sql = "INSERT INTO msa(msa_user, msa_inrouter, msa_inroute, msa_outrouter, msa_outroute)"
                                . "VALUES (:msa_user, :msa_inrouter, :msa_inroute, :msa_outrouter, :msa_outroute)";

                        $params = array(
                                ":msa_user" => $msa["msa_user"],
                                ":msa_inrouter" => $msa["msa_inrouter"],
                                ":msa_inroute" => $msa["msa_inroute"],
                                ":msa_outrouter" => $msa["msa_outrouter"],
                                ":msa_outroute" => $msa["msa_outroute"]
			);

                        $id = $this->db->insert($sql, array($params));
			
                        if (isset($id) && !empty($id)) {
                                $this->output->add("msa", $id);
                                return true;
                        } else {
                                $this->output->add("msa", -1);
                                return false;
                        }
		}

		/**
		 * Updates an msa entry
		 * @param object with
		 * 	- msa_user (required) username
		 * 	- msa_inrouter (required) the inrouter @see routers in docu
		 * 	- msa_inroute (optional) the parameter for inrouter
		 * 	- msa_outrouter (required) the outrouter @see routers in docu
		 * 	- msa_outroute (optional) the parameter for outrouter
		 * @return true or false
		 */
		public function update($obj) {

			if (!isset($obj["msa_user"]) || empty($obj["msa_user"])) {
				$this->output->add("status", "Username is not set.");
				return false;
			}

			if (!isset($obj["msa_inrouter"]) || empty($obj["msa_inrouter"])) {
				$this->output->add("status", "User inrouter is not set.");
				return false;
			}

			if (!isset($obj["msa_outrouter"]) || empty($obj["msa_outrouter"])) {
				$this->output->add("status", "User outrouter is not set.");
				return false;
			}

			$auth = Auth::getInstance($this->db, $this->output);
			if (!$auth->hasDelegatedUser($obj["msa_user"])) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			if ($obj["msa_inrouter"] == "store") {
				if (!$this->updateUserDatabase($obj, isset($obj["msa_transfer"]))) {
					return false;
				}
			}

			$sql = "UPDATE msa SET msa_inrouter = :msa_inrouter,"
					. "msa_inroute = :msa_inroute,"
					. "msa_outrouter = :msa_outrouter,"
					. "msa_outroute = :msa_outroute "
					. "WHERE msa_user = :msa_user";
			$params = array(
				":msa_user" => $obj["msa_user"],
				":msa_inrouter" => $obj["msa_inrouter"],
				":msa_inroute" => $obj["msa_inroute"],
				":msa_outrouter" => $obj["msa_outrouter"],
				":msa_outroute" => $obj["msa_outroute"]
			);

			$status = $this->db->insert($sql, array($params));

			return isset($status);
		}


		/**
		 * Delete an entry from msa table
		 * @param object with
		 * 	- msa_user username
		 * @return true or false
		 */
		public function delete($obj) {

			if (!isset($obj["msa_user"]) || empty($obj["msa_user"])) {
				$this->output->add("status", "Username is not set.");
				return false;
			}

			$auth = Auth::getInstance($this->db, $this->output);
			if (!$auth->hasDelegatedUser($obj["msa_user"])) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "DELETE FROM msa WHERE msa_user = :msa_user";

			$params = array(
				"msa_user" => $obj["msa_user"]
			);

			$status = $this->db->insert($sql, array($params));

			if (isset($status)) {
				return true;
			}

			return false;
		}
	}

?>
