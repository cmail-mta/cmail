<?php
	include_once("../module.php");

	class MSA implements Module {


		private $db;
		private $output;
		// possible endpoints
		private $endPoints = array(
			"get" => "get",
			"add" => "add",
			"update" => "update",
			"delete" => "delete"
		);

		public function __construct($db, $output) {
			$this->db = $db;
			$this->output = $output;
		}

		/**
		 * Return all end points with functions
		 * @return list of end points with (endpoint = key and function name as value)
		 */
		public function getEndPoints() {
			return $this->endPoints;
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
				error_log("addall");
				return $this->getAll();
			}

			$sql = "SELECT * FROM msa WHERE msa_user = :msa_user";

			$params = array(":msa_user" => $obj["msa_user"]);

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
		public function getAll() {

			$sql = "SELECT * FROM msa";

			$out = $this->db->query($sql, array(), DB::F_ARRAY);
			$this->output->add("msa", $out);

			return $out;
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
		public function add($obj) {

                        if (!isset($user["msa_user"]) || empty($user["msa_user"])) {
                                $this->output->add("status", "Username is not set.");
                                return false;
                        }

                        if (!isset($user["msa_inrouter"]) || empty($user["msa_inrouter"])) {
                                $this->output->add("status", "User inrouter is not set.");
                                return false;
                        }

                        if (!isset($user["msa_outrouter"]) || empty($user["msa_outrouter"])) {
                                $this->output->add("status", "User outrouter is not set.");
                                return false;
                        }

                        $sql = "INSERT INTO msa(msa_user, msa_inrouter, msa_inroute, msa_outrouter, msa_outroute)"
                                . "VALUES (:msa_name, :msa_authdata, :msa_inrouter, :msa_inroute, :msa_outrouter, :msa_outroute)";

                        $params = array(
                                ":msa_user" => $user["msa_user"],
                                ":msa_inrouter" => $user["msa_inrouter"],
                                ":msa_inroute" => $user["msa_inroute"],
                                ":msa_outrouter" => $user["msa_outrouter"],
                                ":msa_outroute" => $user["msa_outroute"]
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

			if (isset($status)) {
				return true;
			}
			return false;
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
