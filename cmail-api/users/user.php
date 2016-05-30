<?php

	include_once("../module.php");

	/**
	 * Class for user related things.
	 */
	class User implements Module {

		private $db;
		private $output;
		private $auth;
		private $c;

		// List of end points. Format is:
		// $name => $func
		// $name name of the end point
		// $func name of the function that is called. Function must be in this class.
		private $endPoints = array(
			"get" => "get",
			"add" => "add",
			"delete" => "delete",
			"set_password" => "set_password",
			"delete_permission" => "deletePermission",
			"add_permission" => "addPermission",
			"update_permissions" => "updatePermissions",
			"update_alias" => "updateAlias"
		);

		/**
		 * Constructor for the user class.
		 * @param $db the db object
		 * @param $output the output object
		 */
		public function __construct($c) {
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
		 * @see Module::getActiveUsers
		 */
		public function getActiveUsers() {
			$sql = "SELECT user_name FROM users WHERE user_authdata IS NOT NULL";

			$users = $this->c->getDB()->query($sql, array(), DB::F_ARRAY);

			$output = array();

			foreach($users as $user) {
				$output[$user["user_name"]] = true;
			}

			return $output;
		}

		/**
		 * Returns a list of users that are delegated to the authorized user. In list is the user itself.
		 * @param $write if true send user list to output module
		 * @return list of users
		 */
		private function getDelegated($write = true) {

			$sql = "SELECT user_name, (user_authdata IS NOT NULL) AS user_login, user_alias
				FROM users WHERE user_name IN (
					SELECT api_delegate AS user_name FROM api_user_delegates WHERE api_user = :api_user
				) OR user_name = :api_user";

			$params = array(
				":api_user" => $this->auth->getUser()
			);

			$out = $this->db->query($sql, $params, DB::F_ARRAY);

			$output = [];

			$list = $this->getModuleUserLists();
			foreach($out as $user) {
				$user["modules"] = $this->getActiveModules($user, $list);
				$output[] = $user;
			}

			if ($write) {
				$this->output->add("users", $output);
			}
			return $output;
		}

		/**
		 * Returns the given user when in database. If no user is defined, send all users.
		 * @param obj object with key username into
		 * @param $write (optional) if true give user list to the output module
		 * @output_flags users all users that matches this username (should be one)
		 * @return list of users that matches (should be one)
		 */
		public function get($obj, $write = true) {

			if ($this->auth->hasPermission("admin")) {
				if (!isset($obj["username"])) {
					// if no username is set, return all users
					return $this->getAll();
				} else {
					return $this->getByUser($obj["username"], $write);
				}
			} else if ($this->auth->hasPermission("delegate")) {
				if (isset($obj["username"]) && !empty($obj["username"])) {
					$users = $this->auth->getDelegateUsers();
					$users[] = $this->auth->getUser();
					foreach($users as $user) {
						if ($user == $obj["username"]) {
							return $this->getByUser($user);
						}
					}
					$this->output->add("status", "User has no permission to do this (not in delegated list).");
					$this->output->add("users", []);
					return [];
				} else {
					return $this->getDelegated($write);
				}
			} else {
				if (!isset($obj["username"]) || $obj["username"] == $this->auth->getUser()) {
					return $this->getByUser($this->auth->getUser());
				} else {
					$this->output->add("status","User has no permission to do this (not the user).");
					$this->output->add("users", []);
					return [];
				}
			}

		}

		/**
		 * Return a user by his name
		 * @param $username name of the user
		 * @param $write (optional) if true user object is given to output module
		 * @return user object
		 */
		private function getByUser($username, $write = true) {

			$sql = "SELECT user_name, (user_authdata IS NOT NULL) AS user_login, user_alias FROM users WHERE user_name = :user_name";

			$params = array(":user_name" => $username);

			$out = $this->db->query($sql, $params, DB::F_ARRAY);

			if (count($out) < 1) {
				if ($write) {
					$this->output->add("users", []);
				}
				return [];
			}

			$out["modules"] = $this->getActiveModules($out[0], $this->getModuleUserLists());

			$permission_sql = "SELECT api_permission FROM api_access WHERE api_user = :api_user";
			$permission_params = array(
				":api_user" => $username
			);

			$permissions = $this->db->query($permission_sql, $permission_params, DB::F_ARRAY);
			$out[0]["user_permissions"] = [];

			foreach($permissions as $permission) {
				$out[0]["user_permissions"][] = $permission["api_permission"];
			}

			if ($write) {
				$this->output->add("users", $out);
			}

			return $out;
		}

		/**
		 * Revokes a permission from the given user.
		 * @param $user object with
		 * 	"user_name"  => name of the user
		 * 	"user_permission" => name of the permission
		 * @return true or false
		 */
		public function deletePermission($user) {


			if (!isset($user["user_name"]) || empty($user["user_name"])) {
				$this->output->add("status", "No user is set.");
				return false;
			}

			if (!isset($user["user_permission"]) || empty($user["user_permission"])) {
				$this->output->add("status", "No permission is set.");
				return false;
			}

			if (!$this->auth->hasPermission("admin")) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "DELETE FROM api_access WHERE :api_user = api_user AND api_permission = :api_permission)";

			$params = array(
				":api_user" => $user["user_name"],
				":api_permission" => $user["user_permission"]
			);

			return $this->db->insert($sql, [$params]) > 0;
		}

		/**
		 * grant a permission for the given user.
		 * @param $user object with
		 * 	"user_name" => name of the user
		 * 	"user_permission" => name of the permission
		 * @return true or false
		 */
		public function addPermission($user) {


			if (!isset($user["user_name"]) || empty($user["user_name"])) {
				$this->output->add("status", "No user is set.");
				return false;
			}

			if (!isset($user["user_permission"]) || empty($user["user_permission"])) {
				$this->output->add("status", "No permission is set.");
				return false;
			}

			if (!$this->auth->hasPermission("admin")) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "INSERT INTO api_access (api_user, api_permission) VALUES (:api_user, :api_permission)";

			$params = array(
				":api_user" => $user["user_name"],
				":api_permission" => $user["user_permission"]
			);

			return $this->db->insert($sql, [$params]) > 0;
		}

		/**
		 * Updates the permissions of the given user
		 * @param $user object with
		 * 	"user_name"   => name of the user
		 * 	"user_permissions" => list of user permissions
		 */
		public function updatePermissions($user) {
			if (!isset($user["user_name"]) || empty($user["user_name"])) {
				$this->output->add("status", "No user is set.");
				return false;
			}

			if (!isset($user["user_permissions"])) {
				$this->output->add("status", "No permissions is set.");
				return false;
			}

			if (!$this->auth->hasPermission("admin")) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "DELETE FROM api_access WHERE api_user = :api_user";

			$params = array(
				":api_user" => $user["user_name"]
			);

			$this->db->beginTransaction();

			if ($this->db->insert($sql, [$params]) < 0) {
				$this->db->rollback();
				return;
			}

			foreach($user["user_permissions"] as $permission) {
				if (!$this->addPermission(array(
					"user_permission" => $permission,
					"user_name" => $user["user_name"]
				))) {
					$this->db->rollback();
					return;
				}
			}

			$this->db->commit();
		}

		/**
		 * Return a list of active modules for the given user
		 * @param user object with
		 * 	"user_name" => name of the user
		 * @return list of active modules
		 */
		public function getActiveModules($user, $list) {
			$modules = array();

			foreach ($list as $name => $users) {
				if (isset($users[$user["user_name"]])) {
					$modules[$name] = true;
				}
			}

			return $modules;
		}

		/**
		 * Return if the module is active for the user
		 * @param $username username
		 * @return true or false
		 */
		public function isActive($username) {

			if (!isset($username) || empty($username)) {
				return false;
			}

			$obj = array("username" => $username);

			return ($this->get($obj, false)["user_can_login"]);
		}

		// get list of active users per module
		private function getModuleUserLists() {

			global $modulelist;

			$list = array();
			foreach($modulelist as $module => $path) {
				$list[$module] = getModuleInstance($module, $this->c)->getActiveUsers();
			}
			return $list;
		}

		/**
		 * Returns all users in database.
		 * @output_flags users the userlist
		 * @return list of users
		 */
		public function getAll() {

			$sql = "SELECT user_name, user_authdata IS NOT NULL AS has_login, user_alias, user_database, link_count, coalesce(mails, 0) AS mails FROM users LEFT JOIN (SELECT address_route, count(address_route) AS link_count FROM addresses WHERE address_router = 'store' GROUP BY address_route) ON (address_route = user_name) LEFT JOIN (SELECT mail_user, count(*) AS mails FROM mailbox GROUP BY mail_user) ON (user_name = mail_user)";

			$out = $this->db->query($sql, array(), DB::F_ARRAY);
			$list = $this->getModuleUserLists();
			foreach($out as $key => $user) {
				$out[$key]["modules"] = $this->getActiveModules($user, $list);
			}

			$this->output->add("users", $out);

			return $out;

		}

		/**
		 * create a password hash.
		 * @param $salt salt for the password. If null then a random one will taken.
		 * @param $password the password
		 * @return if salt is null then $salt:sha265($salt, $password), else sha265($salt, $password);
		 */
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
		 * Adds an delegate to the given user.
		 * @param $obj object with
		 * 	"api_user"	=> name of the user
		 * 	"api_delegate"  => name of the delegated user
		 * @param $delegated flag for adding this delegate from the user add call.
		 * @return true or false
		 */
		public function addDelegate($obj, $delegated = false) {

			if (!isset($obj["api_user"]) || empty($obj["api_user"])) {
				$this->output->add("status", "User is not set.");
				return false;
			}

			if (!isset($obj["api_delegate"]) || empty($obj["api_delegate"])) {
				$this->output->add("status", "Delegated user is not set.");
				return false;
			}
			if (!$this->auth->hasPermission("admin") && !$delegated) {
				return false;
			}

			$sql = "INSERT INTO api_user_delegates (api_user, api_delegate) VALUES (:api_user, :api_delegate)";

			$params = array(
				":api_user" => $auth->getUser(),
				":api_delegate" => $obj["api_delegate"]
			);

			return $this->db->insert($sql, [$params]) > 0;
		}

		/**
		 * Removes a delegated user from the given user.
		 * @param $obj object with
		 * 	"api_user"	=> name of the user
		 * 	"api_delegate"  => name of the delegated user
		 * @return true or false
		 */
		public function removeDelegate($obj) {
			if (!isset($obj["api_user"]) || empty($obj["api_user"])) {
				$this->output->add("status", "User is not set.");
				return false;
			}

			if (!isset($obj["api_delegate"]) || empty($obj["api_delegate"])) {
				$this->output->add("status", "Delegated user is not set.");
				return false;
			}
			if (!$this->auth->hasPermission("admin")) {
				return false;
			}

			$sql = "DELETE FROM api_user_delegates WHERE api_user = :api_user AND api_delegate = :api_delegate";

			$params = array(
				":api_user" => $auth->getUser(),
				":api_delegate" => $obj["api_delegate"]
			);

			return $this->db->insert($sql, [$params]) > 0;

		}

		/**
		 * Adds an delegated address to the given user.
		 * @param $obj object with
		 * 	"api_user"	 => name of the user
		 * 	"api_expression" => address expression
		 * @return true or false;
		 */
		public function addDelegatedAddress($obj) {

			if (!isset($obj["api_user"]) || empty($obj["api_user"])) {
				$this->output->add("status", "User is not set.");
				return false;
			}
			if (!isset($obj["api_expression"]) || empty($obj["api_expression"])) {
				$this->output->add("status", "Address expression is not set.");
				return false;
			}
			if (!$this->auth->hasPermission("admin")) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "INSERT INTO api_address_delegates (api_user, api_expression) VALUES (:api_user, :api_expression)";

			$params = array(
				":api_user" => $obj["api_user"],
				":api_expression" => $obj["api_expression"]
			);

			return $this->db->insert($sql, [$params]) > 0;
		}

		/**
		 * Removes an delegated address.
		 * @param $obj object with
		 * 	"api_user" 	 => user with delegated address
		 * 	"api_expression" => address expression
		 * @return true or false
		 */
		public function removeDelegatedAddress($obj, $write = true) {

			if (!isset($obj["api_user"]) || empty($obj["api_user"])) {
				$this->output->add("status", "User is not set.");
				return false;
			}
			if (!isset($obj["api_expression"]) || empty($obj["api_expression"])) {
				$this->output->add("status", "Address expression is not set.");
				return false;
			}
			if (!$this->auth->hasPermission("admin")) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "DELETE FROM api_address_delegates WHERE api_user = :api_user AND api_expression = :api_expression";

			$params = array(
				":api_user" => $obj["api_user"],
				":api_expression" => $obj["api_expression"]
			);

			return $this->db->insert($sql, [$params]) > 0;
		}

		/**
		 * Adds a user to database.
		 * @param $user the user object. Every user needs at least a name.
		 *              Valid fields are:
		 *              	"user_name"     => name of the user
		 *              	"user_authdata" => password for the user
		 *              	"user_alias"    => alias user
		 *              	"user_permissions"   => array with user_permissions
		 * @return true or false
		 */
		public function add($user) {

			if (!isset($user["user_name"]) || empty($user["user_name"])) {
				$this->output->add("status", "Username is not set.");
				return false;
			}

			if (in_array(strtolower($user["user_name"]), ["main", "temp"])) {
				$this->output->add("status", "Username is not allowed. ");
				return false;
			}

			if (!$this->auth->hasPermission("admin") && !$this->auth->hasPermission("delegate")) {
				$this->output->add("status", "Not allowed");
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

			$this->db->beginTransaction();

			$id = $this->db->insert($sql, array($params));

			if (isset($user["user_permissions"]) && !empty($user["user_permissions"])) {
				foreach($user["user_permissions"] as $permission) {
					$this->addPermission(array(
						"user_permission" => $permission,
						"user_name" => $user["user_name"]
					));
				}
			}

			if (isset($user["user_alias"]) && !empty($user["user_alias"])) {
				if (!$this->updateAlias($user)) {
					$this->db->rollback();
					return false;
				}
			}

			if (isset($id) && !empty($id)) {
				$this->output->add("user", true);

				if ($this->auth->hasPermission("delegate")) {
					$status = $this->addDelegate(array("api_delegate" => $user["user_name"]), true);

					if ($status < 1) {
						$this->db->rollback();
						return false;
					}
				}

				$this->db->commit();
				return true;
			} else {
				$this->output->add("user", false);
				$this->db->rollback();
				return false;
			}

		}

		/**
		 * Sets the password for the given user
		 * @param $user user object with
		 * 	"user_name" => name of the user
		 * 	"user_authdata" => password of the user (if null login is not possible)
		 * @return true or false
		 */
		public function set_password($user) {

			if (!isset($user["user_name"]) || empty($user["user_name"])) {
				$this->output->add("status", "Username is not set.");
				return false;
			}

			if (!$this->auth->hasDelegatedUser($user["user_name"]) && $auth->getUser() !== $user["user_name"]) {
				$this->output->add("status", "Not allowed.");
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
		 * @param $obj object with
		 * 	"user_name" name of the user
		 * @output_flags delete is "ok" when everything is fine, else "not ok"
		 * @return false on error, else true
		 */
		public function delete($obj) {
			if (!isset($obj["user_name"]) || empty($obj["user_name"])) {
				$this->output->add("status", "No username set.");
				return false;
			}

			if (!$this->auth->hasDelegatedUser($obj["user_name"]) && $auth->getUser() !== $obj["user_name"]) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "DELETE FROM users WHERE user_name = :username";

			$params = array(":username" => $obj["user_name"]);

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
		 * Updates the alias of the given user.
		 * @param $obj object with
		 * 	"user_name" => name of the user
		 * 	"user_alias" => alias for this user.
		 * @return true or false
		 */
		public function updateAlias($obj) {

			if (!isset($obj["user_name"]) || empty($obj["user_name"])) {
				$this->output->add("status", "No username set.");
				return false;
			}

			if (!isset($obj["user_alias"]) || empty($obj["user_alias"])) {
				$this->output->add("status", "No alias set.");
				return false;
			}

			if ($obj["user_name"] == $obj["user_alias"]) {
				$this->output->add("status", "Cannot alias to the same user.");
				return false;
			}

			if (!$this->auth->hasPermission("admin")) {
				$this->output->add("status", "Not allowed.");
				return false;
			}

			$sql = "UPDATE users SET user_alias = :alias WHERE user_name = :user";

			$params = [
				":alias" => $obj["user_alias"],
				":user" => $obj["user_name"]
			];

			$status = $this->db->insert($sql, [$params]);

			return $status > 0;
		}

	}

?>
