<?php
	/**
	 * This file contains the Delegate Module.
	 * @author Jan Düpmeier <j.duepmeier@googlemail.com>
	 */

	include_once('../module.php');

	/**
	 * Class for delegate related things.
	 */
	class Delegate implements Module {

		/**
		 * DB object
		 */
		private $db;
		/**
		 * Output object.
		 */
		private $output;
		/**
		 * Controller object.
		 */
		private $c;
		/**
		 * Auth object
		 */
		private $auth;

		/**
		 * List of end points. Format is:
		 * $name => $func
		 * $name name of the end point
		 * $func name of the function that is called. Function must be in this class.
		 */
		private $endPoints = [
			'get' => 'get',
			'user_add' => 'addUser',
			'user_delete' => 'deleteUser',
			'address_add' => 'addAddress',
			'address_delete' => 'deleteAddress'
		];

		/**
		 * Constructor for the user class.
		 * @param Controller $c the controller object
		 */
		public function __construct(Controller $c) {
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
		 * Returns the active users of this module.
		 * @return array list of users.
		 */
		public function getActiveUsers() {
			$sql = "SELECT api_user
					FROM api_user_delegates
					UNION SELECT api_user
					FROM api_address_delegates";

			$users = $this->c->getDB()->query($sql, [], DB::F_ARRAY);

			$output = [];

			foreach($users as $user) {
				$output[$user['api_user']] = true;
			}

			return $output;
		}

		/**
		 * Returns all delegated users and all delegated addresses of the user
		 * @param array $obj post object.
		 * @param boolean $write (optional) if true give delegates to the output module
		 * @return object with
		 * 	users list of delegated users
		 * 	addresses list of delegated addresses
		 */
		public function get(array $obj, $write = true) {

			if ($this->auth->hasPermission('admin')) {
				$sql_user = "SELECT * FROM api_user_delegates";
				$sql_addresses = "SELECT * FROM api_address_delegates";
				$params = [];
			} else if ($this->auth->hasPermission('delegate')) {
				$sql_user = "SELECT * FROM api_user_delegates WHERE api_user = :api_user";
				$sql_addresses = "SELECT * FROM api_address_delegates WHERE api_user = :api_user";
				$params = array(
					':api_user' => $this->auth->getUser()
				);
			} else {
				$sql_user = "SELECT * FROM api_user_delegates WHERE api_delegate = :api_user";
				$sql_addresses = "SELECT api_user,
									api_expression
								FROM api_address_delegates
								JOIN addresses ON 
									(address_expression = api_expression)
								WHERE address_user = :api_user";
				$params = [
					':api_user' => $this->auth->getUser()
				];
			}

			$delegates = [
				'users' => $this->db->query($sql_user, $params, DB::F_ARRAY),
				'addresses' => $this->db->query($sql_addresses, $params, DB::F_ARRAY)
			];

			$this->output->add('delegates', $delegates, $write);

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
						(SELECT count(*)
							FROM api_user_delegates
							WHERE api_delegate = :api_user)
						AS users,
						(SELECT count(*) 
							FROM api_address_delegates
							JOIN addresses ON 
								(api_expression LIKE address_expression)
							WHERE address_user = :api_user)
						AS addresses";

			$params = [
				':api_user' => $username
			];

			$out = $this->db->query($sql, $params, DB::F_ARRAY);

			if (count($out) < 1) {
				return false;
			}

			$this->output->addDebugMessage('delegates', $out);
			return $out[0]['users'] + $out[0]['addresses'] > 0;
		}

		/**
		 * Adds a user
		 * @param array $obj post object.
		 * @param boolean $delegated, if this user is delegated.
		 * @return False on error.
		 */
		public function addUser(array $obj, $delegated = false) {

			if (!isset($obj['api_user']) || empty($obj['api_user'])) {
				$this->output->panic('400', 'api_user is required.', $write);
				return false;
			}

			if (!isset($obj['api_delegate']) || empty($obj['api_delegate'])) {
				$this->output->panic('400', 'api_delegate is required.', $write);
				return false;
			}

			if (!$this->auth->hasPermission('admin') && !$delegated) {
				$this->output->panic('403', 'Forbidden.', $write);
				return false;
			}

			$sql = "INSERT INTO api_user_delegates (api_user, api_delegate) VALUES (:api_user, :api_delegate)";

			$params = [
				':api_user' => $obj['api_user'],
				':api_delegate' => $obj['api_delegate']
			];

			return count($this->db->insert($sql, [$params])) > 0;
		}

		/**
		 * Delete delegation of a user.
		 * @param array $obj post object.
		 * @param boolean $write True writes to output.
		 */
		public function deleteUser(array $obj, $write = true) {
			if (!isset($obj['api_user']) || empty($obj['api_user'])) {
				$this->output->panic('400', 'api_user is required.', $write);
				return false;
			}

			if (!isset($obj['api_delegate']) || empty($obj['api_delegate'])) {
				$this->output->panic('400', 'api_delegate is required.', $write);
				return false;
			}

			if (!$this->auth->hasPermission('admin')) {
				$this->output->panic('403', 'Not allowed.', $write);
				return false;
			}

			$sql = "DELETE FROM api_user_delegates WHERE api_user = :api_user AND api_delegate = :api_delegate";

			$params = [
				':api_user' => $obj['api_user'],
				':api_delegate' => $obj['api_delegate']
			];

			return count($this->db->insert($sql, [$params])) > 0;

		}

		/**
		 * Adds an address delegation.
		 * @param array $obj post object
		 * @param boolean $write True writes to output.
		 * @return false on error.
		 */
		public function addAddress($obj, $write = true) {

			if (!isset($obj['api_user']) || empty($obj['api_user'])) {
				$this->output->panic('400', 'api_user is required.', $write);
				return false;
			}
			if (!isset($obj['api_expression']) || empty($obj['api_expression'])) {
				$this->output->panic('400', 'api_expression is required.', $write);
				return false;
			}
			if (!$this->auth->hasPermission('admin')) {
				$this->output->panic('403', 'Not allowed.', $write);
				return false;
			}

			$sql = "INSERT INTO api_address_delegates
					(api_user, api_expression)
					VALUES (:api_user, :api_expression)";

			$params = [
				':api_user' => $obj['api_user'],
				':api_expression' => $obj['api_expression']
			];

			return count($this->db->insert($sql, [$params])) > 0;
		}

		/**
		 * Deletes an address delegation.
		 * @param array $obj post object.
		 * @param boolean $write True writes to output.
		 * @return False on error.
		 */
		public function deleteAddress($obj, $write = true) {

			if (!isset($obj['api_user']) || empty($obj['api_user'])) {
				$this->output->panic('400', 'api_user is required.', $write);
				return false;
			}
			if (!isset($obj['api_expression']) || empty($obj['api_expression'])) {
				$this->output->panic('400', 'api_expression is required.', $write);
				return false;
			}
			if (!$this->auth->hasPermission('admin')) {
				$this->output->panic('403', 'Not allowed.', $write);
				return false;
			}

			$sql = "DELETE FROM api_address_delegates
					WHERE api_user = :api_user
						AND api_expression = :api_expression";

			$params = [
				':api_user' => $obj['api_user'],
				':api_expression' => $obj['api_expression']
			];

			return count($this->db->insert($sql, [$params])) > 0;
		}
	}
?>
