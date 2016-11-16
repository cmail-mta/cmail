<?php
/**
 * This file contains the authentification stuff.
 * @author Jan Düpmeier <j.duepmeier@googlemail.com>
 */


/**
 * This class handles the authentication of users.
 */
class Auth {

	/**
	 * singleton auth object
	 */
	private static $auth = NULL;
	/**
	 * DB object
	 */
	private $db;
	/**
	 * Output object
	 */
	private $output;
	/**
	 * flag if user is authorized.
	 */
	private $authorized;
	/**
	 * array of permissions for the user.
	 */
	private $permissions;
	/**
	 * the username
	 */
	private $user;
	/**
	 * list of delegated users.
	 */
	private $delegates_user = NULL;
	/**
	 * list of delegated addresses.
	 */
	private $delegates_address = NULL;

	/**
	 * Basic constructors.
	 * @param DB $db database object.
	 * @param Output $output output object.
	 */
	private function __construct(DB $db, Output $output) {

		$this->db = $db;
		$this->output = $output;
		$this->authorized = false;
		$this->permissions = [];
	}

	/**
	 * Returns all attributes that should be saved when serialized.
	 * @return array list of attributes to save.
	 */
	public function __sleep() {

		return ['permissions', 'user', 'authorized'];
	}

	/**
	 * get authentication instance instance
	 * @param DB the database object
	 * @param Output the output object
	 */
	public static function getInstance($db, $output) {

		if (is_null(self::$auth)) {
			if (isset($_SESSION['auth'])) {
				self::$auth = unserialize($_SESSION['auth']);
				self::$auth->setDB($db);
				self::$auth->setOutput($output);
			} else {
				self::$auth = new self($db, $output);
			}
		}
		return self::$auth;
	}

	/**
	 * Sets the database object.
	 * @param DB database object to set.
	 */
	public function setDB(DB $db) {
		$this->db = $db;
	}

	/**
	 * Sets the output object.
	 * @param Output output object to set.
	 */
	public function setOutput(Output $output) {
		$this->output = $output;
	}

	/**
	 * Returns the current logged in user.
	 * @return the current logged in user.
	 */
	public function getUser() {
		return $this->user;
	}

	/**
	 * Checks if the authorized users has the given permission
	 * @param permission to check
	 * @return true or false
	 */
	public function hasPermission($permission) {

		return (isset($this->permissions[$permission]) && $this->permissions[$permission]);
	}

	/**
	 * Returns the status of authorization.
	 * @return true if user is authorized, else false
	 */
	public function isAuthorized() {
		return $this->authorized;
	}

	/**
	 * Checks if the current user can manage the given user.
	 * @param The user to manage
	 * @return True if the current user can manage the given one, else false.
	 */
	public function hasDelegatedUser($user) {

		if (!$this->isAuthorized()) {
			return false;
		}

		if ($this->hasPermission('admin')) {
			return true;
		}

		if (!$this->hasPermission('delegate')) {
			return false;
		}

		if ($user === $this->getUser()) {
			return true;
		}

		// check if users is delegated
		$users = $this->getDelegateUsers();

		foreach($users as $delegate) {
			if ($delegate === $user) {
				return true;
			}
		}

		return false;
	}

	/**
	 * Returns if the current user can manage this address(range).
	 * @param Address(range) to manage.
	 * @return True if the current user can manage this address(range).
	 */
	public function hasDelegatedAddress($address) {
		if (!$this->isAuthorized()) {
			return false;
		}

		if ($this->hasPermission('admin')) {
			return true;
		}

		if (!$this->hasPermission('delegate')) {
			return false;
		}

		$sql = "SELECT count(*) AS test FROM api_address_delegates WHERE :address LIKE api_expression";

		$params = [
			':address' => $address
		];

		$out = $this->db->query($sql, $params, DB::F_SINGLE_ASSOC);

		return ($out['test'] > 0);

	}

	/**
	 * Return a list with all users that are delegated to the current authorized user
	 * @return list of users
	 */
	public function getDelegateUsers() {
		// cache?
		if (!is_null($this->delegates_user)) {
			return $this->delegates_user;
		}

		// not allowed?
		if (!$this->isAuthorized()) {
			return [];
		}

		$sql = "SELECT api_delegate FROM api_user_delegates WHERE api_user = :api_user";

		$params = [
			':api_user' => $this->user
		];

		$delegates = $this->db->query($sql, $params, DB::F_ARRAY);

		$output = [];
		foreach($delegates as $delegate) {
			$output[] = $delegate["api_delegate"];
		}
		$this->delegates_user = $output;
		return $output;

	}

	/**
	 * Returns all address expressions that are delegated to the current authorized user
	 * @return list of address expressions
	 */
	public function getDelegateAddresses() {

		// cache
		if (!is_null($this->delegates_addresses)) {
			return $this->delegates_addresses;
		}

		// not allowed?
		if (!$this->isAuthorized()) {
			return [];
		}

		$sql = "SELECT api_expression FROM api_address_delegates WHERE api_user = :api_user";

		$params = [
			':api_user' => $this->user
		];

		$delegates = $this->db->query($sql, $params, DB::F_ARRAY);

		$output = [];
		foreach($delegates as $delegate) {
			$output[] = $delegate['api_expression'];
		}
		$this->delegates_addresses = $output;
		return $output;

	}

	/**
	 * Return all permissions of this user.
	 * @return list of permissions.
	 */
	private function getPermissions() {

		$sql = "SELECT api_permission FROM api_access WHERE api_user = :api_user";

		$params = [
			':api_user' => $this->user
		];

		$out = $this->db->query($sql, $params, DB::F_ARRAY);

		foreach($out as $permission) {
			$this->permissions[$permission['api_permission']] = true;
		}

		$this->output->add('permissions', $this->permissions);
		return $this->permissions;
	}

	/**
	 * authorization function
	 * @param db database connection
	 * @param output output object
	 * @param auth object with authorization data (like user, pw)
	 * @return true if ok, else false
	 */
	function auth($auth) {

		$this->output->add('session', isset($_SERVER['auth']));

		// init logout
		if (isset($_GET['logout'])) {
			session_destroy();
			session_regenerate_id(true);
			$this->output->add('status', 'Successfully logged out.');
			return false;
		}

		// cache
		if ($this->authorized) {
			$this->output->add('permissions', $this->permissions);
			return true;
		}

		if (!isset($auth)) {
			$this->output->add('status', 'Please login first');
			return false;
		}
		if (!isset($auth['user_name']) || empty($auth['user_name'])) {
			$this->output->add('status', 'Username is not set!');
			return false;
		}

		if (!isset($auth['password']) || empty($auth['password'])) {
			$this->output->add('status', 'Password is not set!');
			return false;
		}

		$sql = "SELECT user_authdata FROM users WHERE user_name = :user_name";

		$params = [
			':user_name' => $auth['user_name']
		];

		$user = $this->db->query($sql, $params, DB::F_ARRAY);

		if (count($user) < 1) {
			$this->output->add('status', 'Username not found.');
			return false;
		}

		$user = $user[0];
		$this->user = $auth['user_name'];

		if (!isset($user['user_authdata'])) {
			$this->output->add('status', 'User is not allowed to login.');
			return false;
		}

		$password = explode(":", $user['user_authdata']);

		$pass = hash('sha256', $password[0] . $auth['password']);

		$this->authorized = ($password[1] === $pass);

		if ($this->authorized) {
			$this->getPermissions();
			$_SESSION['auth'] = serialize($this);
		}

		return $this->authorized;
	}
}
