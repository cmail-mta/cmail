<?php


class Auth {

	private static $auth = NULL;
	private $db;
	private $output;
	private $authorized;
	private $rights;
	private $user;
	private $delegates_user = NULL;
	private $delegates_address = NULL;

	private function __construct($db, $output) {

		$this->db = $db;
		$this->output = $output;
		$this->authorized = false;
		$this->rights = array();
	}

	public function __sleep() {

		return array("rights", "user", "delegates_user", "delegates_address", "authorized");
	}

	/**
	 * get auth instance
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

	public function setDB($db) {
		$this->db = $db;
	}

	public function setOutput($output) {
		$this->output = $output;
	}

	public function getUser() {
		return $this->user;
	}

	/**
	 * Checks if the authorized users has the given right
	 * @param right to check
	 * @returns true or false
	 */
	public function hasRight($right) {

		if (isset($this->rights[$right]) && $this->rights[$right]) {
			return true;
		}

		return false;
	}

	/**
	 * Returns the status of authorization
	 * @returns true if user is authorized, else false
	 */
	public function isAuthorized() {
		return $this->authorized;
	}

	public function hasDelegatedUser($user) {

		if (!$this->isAuthorized()) {
			return false;
		}

		if ($this->hasRight("admin")) {
			return true;
		}

		if (!$this->hasRight("delegate")) {
			return false;
		}

		$users = $this->getDelegateUsers();

		foreach($users as $delegate) {
			if ($delegate === $user) {
				return true;
			}
		}

		return false;
	}

	/**
	 * Return a list with all users that are delegated to the current authorized user
	 * @return list of users
	 */
	public function getDelegateUsers() {

		if (!is_null($this->delegates_user)) {
			return $this->delegates_user;
		}

		if (!$this->isAuthorized()) {
			return array();
		}
		
		$sql = "SELECT api_delegate FROM api_user_delegates WHERE api_user = :api_user";

		$params = array(
			":api_user" => $this->user
		);

		$delegates = $this->db->query($sql, $params, DB::F_ARRAY);

		$output = array();
		foreach($delegates as $delegate) {
			$output[] = $delegate["api_delegate"];
		}
		$this->delegates_user = $output;
		$_SESSION['auth'] = serialize($this);
		return $output;

	}

	/**
	 * Returns all address expressions that are delegated to the current authorized user
	 * @return list of address expressions
	 */
	public function getDelegateAddresses() {

		if (!is_null($this->delegates_addresses)) {
			return $this->delegates_addresses;
		}

		if (!$this->isAuthorized()) {
			return array();
		}
		
		$sql = "SELECT api_expression FROM api_address_delegates WHERE api_user = :api_user";

		$params = array(
			":api_user" => $this->user
		);

		$delegates = $this->db->query($sql, $params, DB::F_ARRAY);

		$output = array();
		foreach($delegates as $delegate) {
			$output[] = $delegate["api_expression"];
		}
		$this->delegates_addresses = $output;
		$_SESSION['auth'] = serialize($this);
		return $output;

	}

	private function getRights() {

		$sql = "SELECT api_right FROM api_access WHERE api_user = :api_user";

		$params = array(
			":api_user" => $this->user
		);

		$out = $this->db->query($sql, $params, DB::F_ARRAY);

		foreach($out as $right) {
			$this->rights[$right["api_right"]] = true;
		}

		$this->output->add("rights", $this->rights);
	}

	/**
	 * authorization function
	 * @param db database connection
	 * @param output output object
	 * @param auth object with authorization data (like user, pw)
	 * @return true if ok, else false
	 */
	function auth($auth) {

		$this->output->add("session", isset($_SERVER['auth']));	

		if (isset($auth["logout"])) {
			return false;
		}
		
		if ($this->authorized) {
			
			$this->output->add("rights", $this->rights);
			return true;
		}
		
		$auth = array();
		if (!isset($_SERVER["PHP_AUTH_USER"]) || empty($_SERVER["PHP_AUTH_USER"])) {
			return false;
		}

		if (!isset($_SERVER["PHP_AUTH_PW"]) || empty($_SERVER["PHP_AUTH_PW"])) {
			return false;
		}

		$auth["user_name"] = $_SERVER["PHP_AUTH_USER"];
		$auth["password"] = $_SERVER["PHP_AUTH_PW"];

		if (!isset($auth["user_name"]) || empty($auth["user_name"]) ) {
			$this->output->add("status", "No auth username set.");
			return false;
		}

		$sql = "SELECT user_authdata FROM users WHERE user_name = :user_name";

		$params = array(
			":user_name" => $auth["user_name"]
		);

		$user = $this->db->query($sql, $params, DB::F_ARRAY);


		if (count($user) < 1) {
			$this->output->add("status", "Username not found.");
			return false;	
		}

		$user = $user[0];
		$this->user = $auth["user_name"];

		if (!isset($user["user_authdata"])) {
			$this->output->add("status", "User is not allowed to login.");
			return false;
		}

		$password = explode(":", $user["user_authdata"]);

		$pass = hash("sha256", $password[0] . $auth["password"]);

		$this->authorized = ($password[1] === $pass);

		if ($this->authorized) {
			$this->getRights();
			$_SESSION['auth'] = serialize($this);
		}

		return $this->authorized;
	}

}

?>
