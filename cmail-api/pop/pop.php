<?php
	require_once("../module.php");

	class POP implements Module {


		private $db;
		private $output;
		private $endPoints = array(
			"get" => "get",
			"add" => "add",
			"delete" => "delete"
		);

		public function __construct($db, $output) {
			$this->db = $db;
			$this->output = $output;
		}

		/**
		 * Returns true if the given user is in userlist (can use pop)
		 * @param $username name of the user
		 * @return true or false
		 */
		public function isActive($username) {

			if (!isset($username) || empty($username)) {
				return false;
			}

			$users = $this->get(array("pop_user" => $username), false);

			if (count($users) == 1) {
				return true;
			}
			return false;
		}

		public function getEndPoints() {
			return $this->endPoints;
		}

		private function getDelegated($write = true) {

			$auth = Auth::getInstance($this->db, $this->output);

			$sql = "SELECT pop_user, pop_lock FROM popd WHERE pop_user = :api_user OR pop_user IN 
				(SELECT api_delegate FROM api_user_delegates WHERE api_user = :api_user)";

			$params = array(
				":api_user" => $auth->getUser()
			);

			$out = $this->db->query($sql, $params, DB::F_ARRAY);

			if ($write) {
				$this->output->add("pop", $out);
			}

			return $out;
		}

		/**
		 * Returns all users in psend list to output module.
		 * @param $write if true
		 * @return list of users
		 */
		public function getAll($write = true) {

			$auth = Auth::getInstance($this->db, $this->output);

			if (!$auth->hasRight("admin")) {
				if ($auth->hasRight("delegate")) {
					return $this->getDelegated();
				} else {
					return $this->getByUser($auth->getUser());
				}
			}

			$sql = "SELECT pop_user, pop_lock FROM popd";

			$out = $this->db->query($sql, array(), DB::F_ARRAY);

			$this->output->add("pop", $out);
			return $out;
		}


		/**
		 * Returns a list with user(s). If a username is defined the list
		 * contains only the given user
		 * @param object with
		 * 	pop_user (optional) = the username
		 * @return list of user(s)
		 */
		public function get($obj, $write = true) {

			if (!isset($obj["pop_user"]) || empty($obj["pop_user"])) {
				
				//getall
				return $this->getAll();
			}

			return $this->getByUser($obj["pop_user"], $write);

		}

		private function getByUser($username, $write = true) {


			$auth = Auth::getInstance($this->db, $this->output);
			
			if (!$auth->hasDelegatedUser($obj["pop_user"]) && ($auth->getUser() !== $obj["pop_user"])) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "SELECT pop_user, pop_lock FROM popd WHERE pop_user = :pop_user";
			$params = array(
				":pop_user" => $username
			);

			$out = $this->db->query($sql, $params, DB::F_ARRAY);

			if ($write) {
				$this->output->add("pop", $out);
			}
			return $out;
		}


		/**
		 * Deletes the given user from popd table (Disables pop)
		 * @param object with
		 * 	pop_user = the username
		 * @return true on success, false on error
		 */
		public function delete($obj) {
			if (!isset($obj["pop_user"]) || empty($obj["pop_user"])) {
				$this->output->add("status", "No username is set");
				return false;
			}

			$auth = Auth::getInstance($this->db, $this->output);

			if (!$auth->hasDelegatedUser($obj["pop_user"]) && ($auth->getUser() !== $obj["pop_user"])) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "DELETE FROM popd WHERE pop_user = :pop_user";

			$params = array(
				":pop_user" => $obj["pop_user"]
			);

			$status = $this->db->insert($sql, [$params]);

			if (isset($status)) {
				$this->output->add("status", "ok");
				return true;
			} else {
				return false;
			}
		}


		/**
		 * Adds the given user to the popd table (Enables pop)
		 * @param object with
		 * 	pop_user = the username
		 * @return true on success, false on error
		 */
		public function add($obj) {
			if (!isset($obj["pop_user"]) || empty($obj["pop_user"])) {
				$this->output->add("status", "No username is set");
				return false;
			}
			$auth = Auth::getInstance($this->db, $this->output);

			if (!$auth->hasDelegatedUser($obj["pop_user"]) && ($auth->getUser() !== $obj["pop_user"])) {
				$this->output->add("status", "Not allowed.");
				return false;
			}
			$sql = "INSERT INTO popd (pop_user) VALUES (:pop_user)";
			$params = array(
				":pop_user" => $obj["pop_user"]
			);

			$status = $this->db->insert($sql, [$params]);

			if (isset($status)) {
				$this->output->add("status", "ok");
				return true;
			} else {
				return false;
			}
		}

	}

?>
