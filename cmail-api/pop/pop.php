<?php
	/**
	 * This file contains the POP-Module.
	 * @author Jan Düpmeier <j.duepmeier@googlemail.com>
	 */

	require_once('../module.php');

	/**
	 * Implements the POP module.
	 */
	class POP implements Module {

		/**
		 * Database object.
		 */
		private $db;
		/**
		 * Output object.
		 */
		private $output;
		/**
		 * Aauth object.
		 */
		private $auth;
		/**
		 * Controller object.
		 */
		private $c;
		/**
		 * Module endpoints.
		 */
		private $endPoints = [
			'get' => 'get',
			'add' => 'add',
			'delete' => 'delete',
			'unlock' => 'unlock'
		];

		/**
		 * Constructs this module.
		 * @param Controller $c the controller object
		 */
		public function __construct(Controller $c) {
			$this->c = $c;
			$this->db = $c->getDB();
			$this->auth = $c->getAuth();
			$this->output = $c->getOutput();
		}

		/**
		 * Returns true if the given user is in userlist (can use pop)
		 * @param string $username name of the user
		 * @return true if the module is active for this user.
		 */
		public function isActive($username) {

			if (empty($username)) {
				return false;
			}

			$users = $this->get(['pop_user' => $username], false);

			return count($users) === 1;
		}

		/**
		 * Returns all endpoints of this module
		 * @return array of endpoints
		 */
		public function getEndPoints() {
			return $this->endPoints;
		}

		/**
		 * Returns all users who had access with pop.
		 * @return array of users
		 */
		public function getActiveUsers() {

			$sql = "SELECT pop_user FROM popd GROUP BY pop_user";

			$users = $this->c->getDB()->query($sql, [], DB::F_ARRAY);

			$output = [];

			foreach($users as $user) {
				$output[$user['pop_user']] = true;
			}

			return $output;
		}

		/**
		 * Return all pop users with are delegated to the current user.
		 * @param boolean $write True writes to output
		 * @return array of delegated pop users
		 */
		private function getDelegated($write = true) {

			$sql = "SELECT pop_user,
						pop_lock
					FROM popd
					WHERE pop_user = :api_user
						OR pop_user IN
						(SELECT api_delegate FROM api_user_delegates
							WHERE api_user = :api_user)";

			$params = [
				':api_user' => $auth->getUser()
			];

			$out = $this->db->query($sql, $params, DB::F_ARRAY);

			$this->output->add('pop', $out, $write);

			return $out;
		}

		/**
		 * Returns all users in psend list to output module.
		 * @param $write if true
		 * @return list of users
		 */
		public function getAll($write = true) {

			if (!$this->auth->hasPermission('admin')) {
				if ($this->auth->hasPermission('delegate')) {
					return $this->getDelegated();
				} else {
					return $this->getByUser($auth->getUser());
				}
			}

			$sql = "SELECT pop_user, pop_lock FROM popd";

			$out = $this->db->query($sql, [], DB::F_ARRAY);

			$this->output->add('pop', $out, $write);
			return $out;
		}


		/**
		 * Returns a list with user(s). If a username is defined the list
		 * contains only the given user
		 * @param array $obj object with
		 * 	pop_user (optional) = the username
		 * @param boolean $write True writes to output.
		 * @return list of user(s)
		 */
		public function get(array $obj, $write = true) {

			if (empty($obj['pop_user'])) {

				//getall
				return $this->getAll();
			}

			return $this->getByUser($obj['pop_user'], $write);
		}

		/**
		 * Returns the user by its name.
		 * @param string $username name of the user
		 * @param boolean $write True writes to output.
		 * @return array array with the user entry.
		 */
		private function getByUser($username, $write = true) {

			if (!$this->auth->hasDelegatedUser($username) && ($this->auth->getUser() !== $username)) {
				$this->output->panic('403', 'Not allowed.', $write);
				return [];
			}

			$sql = "SELECT pop_user, pop_lock FROM popd WHERE pop_user = :pop_user";
			$params = [
				':pop_user' => $username
			];

			$out = $this->db->query($sql, $params, DB::F_ARRAY);

			$this->output->add('pop', $out, $write);
			return $out;
		}


		/**
		 * Deletes the given user from popd table (Disables pop)
		 * @param array $obj object with
		 * 	pop_user = the username
		 * @param boolean $write True writes to output.
		 * @return true on success, false on error
		 */
		public function delete(array $obj, $write = true) {
			if (empty($obj['pop_user'])) {
				$this->output->panic('400', 'pop_user is required.', $write);
				return false;
			}

			if (!$this->auth->hasDelegatedUser($obj['pop_user']) && ($this->auth->getUser() !== $obj['pop_user'])) {
				$this->output->panic('403', 'Not allowed.', $write);
				return false;
			}

			$sql = "DELETE FROM popd WHERE pop_user = :pop_user";

			$params = [
				':pop_user' => $obj['pop_user']
			];

			$status = $this->db->insert($sql, [$params]);

			if (count($status) > 0) {
				$this->output->add('status', 'ok', $write);
				return true;
			} else {
				return false;
			}
		}


		/**
		 * Unlocks the pop account.
		 * @param array $obj post object.
		 * @param boolean $write True writes to output.
		 * @return false on error.
		 */
		public function unlock(array $obj, $write = true) {

			if (!isset($obj['pop_user']) || empty($obj['pop_user'])) {
				$this->output->panic('400', 'pop_user is required.', $write);
				return false;
			}
			if (!$this->auth->hasDelegatedUser($obj['pop_user']) && ($this->auth->getUser() !== $obj['pop_user'])) {
				$this->output->panic('403', 'Not allowed.', $write);
				return false;
			}

			$sql = "UPDATE popd SET pop_lock = 0 WHERE pop_user = :pop_user";
			$params = [
				':pop_user' => $obj['pop_user']
			];

			$status = $this->db->insert($sql, [$params]);

			return count($status) > 0;
		}

		/**
		 * Adds the given user to the popd table (Enables pop)
		 * @param array $obj object with
		 * 	pop_user = the username
		 * @param boolean $write True writes to output.
		 * @return true on success, false on error
		 */
		public function add(array $obj, $write = true) {
			if (!isset($obj['pop_user']) || empty($obj['pop_user'])) {
				$this->output->panic('400', 'pop_user is required.', $write);
				return false;
			}

			if (!$this->auth->hasDelegatedUser($obj['pop_user']) && ($this->auth->getUser() !== $obj['pop_user'])) {
				$this->output->panic('403', 'Not allowed.', $write);
				return false;
			}
			$sql = "INSERT INTO popd (pop_user) VALUES (:pop_user)";
			$params = [
				':pop_user' => $obj['pop_user']
			];

			$status = $this->db->insert($sql, [$params]);

			return count($status) > 0;
		}
	}
?>
