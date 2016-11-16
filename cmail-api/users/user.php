<?php
/**
 * This file contains the User module.
 *
 * The user module controls all actions on the user table.
 * @author Jan Düpmeier <j.duepmeier@googlemail.com>
 */

/**
 * Contains the module interface.
 */
require_once("../module.php");
/**
 * Class for user related things.
 */
class User implements Module {

	/**
	 * DB object
	 */
	private $db;
	/**
	 * Output object
	 */
	private $output;
	/**
	 * Auth object
	 */
	private $auth;
	/**
	 * Controller object.
	 */
	private $c;

	/** List of end points. Format is:
	 * $name => $func
	 * $name name of the end point
	 * $func name of the function that is called. Function must be in this class.
	 */
	private $endPoints = [
		'get' => 'get',
		'add' => 'add',
		'delete' => 'delete',
		'set_password' => 'set_password',
		'delete_permission' => 'deletePermission',
		'add_permission' => 'addPermission',
		'update_permissions' => 'updatePermissions',
		'update_alias' => 'updateAlias'
	];

	/**
	 * Constructor for the user class.
	 * @param Controller $c Controller object.
	 */
	public function __construct(Controller $c) {
		$this->c = $c;
		$this->db = $c->getDB();
		$this->output = $c->getOutput();
		$this->auth = $c->getAuth();
	}

	/**
	 * Returns all endpoints for this module.
	 * @return array list of endpoints as keys and function name as values.
	 */
	public function getEndPoints() {
		return $this->endPoints;
	}

	/**
	 * Returns all users that can login.
	 * @return array list of active users.
	 */
	public function getActiveUsers() {
		$sql = "SELECT user_name FROM users WHERE user_authdata IS NOT NULL";

		$users = $this->c->getDB()->query($sql, [], DB::F_ARRAY);

		$output = [];

		foreach($users as $user) {
			$output[$user['user_name']] = true;
		}

		return $output;
	}

	/**
	 * Returns a list of users that are delegated to the authorized user. In list is the user itself.
	 * @param boolean $write True writes to output
	 * @return array list of users
	 */
	private function getDelegated($write = true) {

		$sql = "SELECT user_name,
			(user_authdata IS NOT NULL) AS user_login,
			user_alias
			FROM users
			WHERE user_name IN (
				SELECT api_delegate AS user_name
				FROM api_user_delegates
				WHERE api_user = :api_user
			) OR user_name = :api_user";

$params = [
	':api_user' => $this->auth->getUser()
];

$out = $this->db->query($sql, $params, DB::F_ARRAY);

$output = [];

$list = $this->getModuleUserLists();
foreach($out as $user) {
	$user['modules'] = $this->getActiveModules($user, $list);
	$output[] = $user;
}
$this->output->add('users', $output, $write);
return $output;
	}

	/**
	 * Returns the given user when in database. If no user is defined, return all users.
	 * @param array $obj object
	 * @param boolean $write True writes to output
	 * @return array list of users that matches (should be one)
	 */
	public function get(array $obj, $write = true) {

		if ($this->auth->hasPermission('admin')) {
			if (!isset($obj['username'])) {
				// if no username is set, return all users
				return $this->getAll();
			} else {
				return $this->getByUser($obj['username'], $write);
			}
		} else if ($this->auth->hasPermission('delegate')) {
			if (isset($obj['username']) && !empty($obj['username'])) {
				$users = $this->auth->getDelegateUsers();
				$users[] = $this->auth->getUser();
				foreach($users as $user) {
					if ($user == $obj['username']) {
						return $this->getByUser($user);
					}
				}
				$this->output->panic('403', 'User has no permission to do this (not in delegated list).', $write);
				return [];
			} else {
				return $this->getDelegated($write);
			}
		} else {
			if (!isset($obj['username']) || $obj['username'] == $this->auth->getUser()) {
				return $this->getByUser($this->auth->getUser());
			} else {
				$this->output->panic('403','User has no permission to do this (not the user).', $write);
				return [];
			}
		}

	}

	/**
	 * Return a user by his name
	 * @param string $username name of the user
	 * @param boolean $write True writes to output.
	 * @return array user object
	 */
	private function getByUser($username, $write = true) {

		$sql = "SELECT user_name,
			(user_authdata IS NOT NULL) AS user_login,
			user_alias
			FROM users
			WHERE user_name = :user_name";

		$params = [':user_name' => $username];

		$out = $this->db->query($sql, $params, DB::F_ARRAY);

		if (count($out) < 1) {
			$this->output->add('users', [], $write);
			return [];
		}

		$out['modules'] = $this->getActiveModules($out[0], $this->getModuleUserLists());

		$permission_sql = "SELECT api_permission FROM api_access WHERE api_user = :api_user";
		$permission_params = [
			':api_user' => $username
		];

		$permissions = $this->db->query($permission_sql, $permission_params, DB::F_ARRAY);
		$out[0]['user_permissions'] = [];

		foreach($permissions as $permission) {
			$out[0]['user_permissions'][] = $permission['api_permission'];
		}

		$this->output->add('users', $out, $write);

		return $out;
	}

	/**
	 * Revokes a permission from the given user.
	 * @param array $user object with
	 * 	'user_name'  => name of the user
	 * 	'user_permission' => name of the permission
	 * @param boolean $write True writes to output
	 * @return boolean False on error.
	 */
	public function deletePermission(array $user, $write = true) {

		if (!$this->c->checkParameter($user, ['user_name', 'user_permission'], $write)) {
			return false;
		}

		if (!$this->auth->hasPermission('admin')) {
			$this->output->panic('403', 'Not allowed.', $write);
			return false;
		}

		$sql = "DELETE FROM api_access WHERE :api_user = api_user AND api_permission = :api_permission)";

		$params = [
			':api_user' => $user['user_name'],
			':api_permission' => $user['user_permission']
		];

		return count($this->db->insert($sql, [$params])) > 0;
	}

	/**
	 * grant a permission for the given user.
	 * @param array $user object with
	 * 	'user_name' => name of the user
	 * 	'user_permission' => name of the permission
	 * @return true or false
	 */
	public function addPermission(array $user) {

		if (!$this->c->checkParameter($user, ['user_name', 'user_permission'], $write)) {
			return false;
		}

		if (!$this->auth->hasPermission('admin')) {
			$this->output->panic('403', 'Not allowed.', $write);
			return false;
		}

		$sql = "INSERT INTO api_access (api_user, api_permission) VALUES (:api_user, :api_permission)";

		$params = [
			':api_user' => $user['user_name'],
			':api_permission' => $user['user_permission']
		];

		return count($this->db->insert($sql, [$params])) > 0;
	}

	/**
	 * Updates the permissions of the given user.
	 * @param array $user object with
	 * 	'user_name'   => name of the user
	 * 	'user_permissions' => list of user permissions
	 * @param boolean $write True writes to output.
	 * @return boolean False on error.
	 */
	public function updatePermissions($user, $write = true) {
		if (!$this->c->checkParameter($user, ['user_name', 'user_permissions'], $write)) {
			return false;
		}

		if (!$this->auth->hasPermission('admin')) {
			$this->output->add('403', 'Not allowed.', $write);
			return false;
		}

		$sql = "DELETE FROM api_access WHERE api_user = :api_user";

		$params = [
			':api_user' => $user['user_name']
		];

		$this->db->beginTransaction();

		if (count($this->db->insert($sql, [$params])) < 0) {
			$this->db->rollback();
			return false;
		}

		foreach($user['user_permissions'] as $permission) {
			if (!$this->addPermission([
				'user_permission' => $permission,
				'user_name' => $user['user_name']
			])) {
			$this->db->rollback();
			return false;
			}
		}

		$this->db->commit();

		return true;
	}

	/**
	 * Return a list of active modules for the given user
	 * @param array user object with
	 * 	'user_name' => name of the user
	 * @param array $list map of modules and active users.
	 * @return array list of active modules
	 */
	public function getActiveModules($user, $list) {
		$modules = [];

		foreach ($list as $name => $users) {
			if (isset($users[$user['user_name']])) {
				$modules[$name] = true;
			}
		}

		return $modules;
	}

	/**
	 * Return if the module is active for the user
	 * @param string $username username
	 * @return boolean true when this module is active for this user.
	 */
	public function isActive($username) {

		if (empty($username)) {
			return false;
		}

		$obj = ['username' => $username];

		return ($this->get($obj, false)['user_can_login']);
	}

	/**
	 * Returns all active users for every active module.
	 * @return array map of modules with list of active users.
	 */
	private function getModuleUserLists() {

		global $modulelist;

		$list = [];
		foreach($modulelist as $module => $path) {
			$list[$module] = getModuleInstance($module, $this->c)->getActiveUsers();
		}
		return $list;
	}

	/**
	 * Returns all users in database.
	 * @param boolean $write True writes to output
	 * @return list of users
	 */
	public function getAll($write = true) {

		$sql = "SELECT user_name,
			user_authdata IS NOT NULL AS has_login,
			user_alias,
			user_database,
			link_count,
			coalesce(mails, 0) AS mails
			FROM users
			LEFT JOIN
			(SELECT address_route,
			count(address_route) AS link_count
			FROM addresses
			WHERE address_router = 'store'
			GROUP BY address_route)
			ON (address_route = user_name)
			LEFT JOIN
			(SELECT mail_user,
			count(*) AS mails
			FROM mailbox
			GROUP BY mail_user)
			ON (user_name = mail_user)";

		$out = $this->db->query($sql, [], DB::F_ARRAY);
		$list = $this->getModuleUserLists();

		foreach($out as $key => $user) {
			$out[$key]['modules'] = $this->getActiveModules($user, $list);
		}

		$this->output->add('users', $out, $write);

		return $out;

	}

	/**
	 * create a password hash.
	 * @param string $salt salt for the password. If null then a random one will taken.
	 * @param string $password the password
	 * @return if salt is null then $salt:sha265($salt, $password), else sha265($salt, $password);
	 */
	public function create_password_hash($salt, $password) {

		if (is_null($salt)) {
			$salt = uniqid(mt_rand(), true);
			$hpw = hash('sha256', $salt, $password);
			$hash = "{$salt}:{$hpw}";
			return $hash;
		} else {
			return hash('sha265', $salt . $password);
		}
	}

	/**
	 * Adds an delegate to the given user.
	 * @param array $obj object with
	 * 	'api_user'	=> name of the user
	 * 	'api_delegate'  => name of the delegated user
	 * @param boolean $delegated flag for adding this delegate from the user add call.
	 * @param boolean $write True writes to output.
	 * @return False on error.
	 */
	public function addDelegate($obj, $delegated = false, $write = true) {

		if (!$this->c->checkParameter($obj, ['api_user', 'api_delegate'], $write)) {
			return false;
		}

		if (!$this->auth->hasPermission('admin') && !$delegated) {
			$this->c->getOutput()->panic('403', 'Not allowed.', $write);
			return false;
		}

		$sql = "INSERT INTO api_user_delegates (api_user, api_delegate) VALUES (:api_user, :api_delegate)";

		$params = [
			':api_user' => $auth->getUser(),
			':api_delegate' => $obj['api_delegate']
		];

		return count($this->db->insert($sql, [$params])) > 0;
	}

	/**
	 * Removes a delegated user from the given user.
	 * @param array $obj object with
	 * 	'api_user'	=> name of the user
	 * 	'api_delegate'  => name of the delegated user
	 * @param boolean $write True writes to output.
	 * @return boolean False on error.
	 */
	public function removeDelegate($obj) {
		if (!$this->c->checkParameter($obj, ['api_user', 'api_delegate'], $write)) {
			return false;
		}

		if (!$this->auth->hasPermission('admin')) {
			$this->c->getOutput()->panic('403', 'Not allowed.', $write);
			return false;
		}

		$sql = "DELETE FROM api_user_delegates WHERE api_user = :api_user AND api_delegate = :api_delegate";

		$params = [
			':api_user' => $auth->getUser(),
			':api_delegate' => $obj['api_delegate']
		];

		return count($this->db->insert($sql, [$params])) > 0;
	}

	/**
	 * Adds an delegated address to the given user.
	 * @param array $obj object with
	 * 	'api_user'	 => name of the user
	 * 	'api_expression' => address expression
	 * @param boolean $write True writes to output.
	 * @return boolean False on error.
	 */
	public function addDelegatedAddress(array $obj, $write = true) {

		if (!$this->c->checkParameter($obj, ['api_user', 'api_expression'], $write)) {
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
	 * Removes an delegated address.
	 * @param array $obj object with
	 * 	'api_user' 	 => user with delegated address
	 * 	'api_expression' => address expression
	 * @param boolean $write True writes to output.
	 * @return boolean False on error.
	 */
	public function removeDelegatedAddress(array $obj, $write = true) {

		if (!$this->c->checkParameter($obj, ['api_user', 'api_expression'], $write)) {
			return false;
		}

		if (!$this->auth->hasPermission('admin')) {
			$this->output->add('403', 'Not allowed.', $write);
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

	/**
	 * Adds a user to database.
	 * @param array $user the user object. Every user needs at least a name.
	 *              Valid fields are:
	 *              	'user_name'     => name of the user
	 *              	'user_authdata' => password for the user
	 *              	'user_alias'    => alias user
	 *              	'user_permissions'   => array with user_permissions
	 * @param boolean $write True writes to output.
	 * @return boolean False on error.
	 */
	public function add(array $user, $write = false) {

		if (!$this->c->checkParameter($obj, ['user_name'], $write)) {
			return false;
		}

		if (!$this->auth->hasPermission('admin') && !$this->auth->hasPermission('delegate')) {
			$this->output->panic('403', 'Not allowed', $write);
			return false;
		}

		if (in_array(strtolower($user['user_name']), ['main', 'temp'])) {
			$this->output->panic('400', "Username {$user['user_name']} is not allowed.", $write);
			return false;
		}

		if (!empty($user['user_authdata']) && $user['user_authdata'] !== '') {
			$user['user_authdata'] = $this->create_password_hash(null, $user['user_authdata']);
		} else {
			$user['user_authdata'] = null;
		}


		$sql = "INSERT INTO users(user_name, user_authdata) VALUES (:user_name, :user_authdata)";

		$params = [
			':user_name' => $user['user_name'],
			':user_authdata' => $user['user_authdata'],
		];

		$this->db->beginTransaction();

		$ids = $this->db->insert($sql, [$params]);
		if (count($ids) < 1) {
			return false;
		}

		$id = $ids[0];

		if (!empty($user['user_permissions'])) {
			foreach($user['user_permissions'] as $permission) {
				if ($this->addPermission([
					'user_permission' => $permission,
					'user_name' => $user['user_name']
				])) {
				$this->db->rollback();
				return false;
				}
			}
		}

		if (!empty($user['user_alias'])) {
			if (!$this->updateAlias($user)) {
				$this->db->rollback();
				return false;
			}
		}

		if (!empty($id)) {
			$this->output->add('user', true, $write);

			if ($this->auth->hasPermission('delegate')) {
				if (!$this->addDelegate(['api_delegate' => $user['user_name']], true, $write)) {
					$this->db->rollback();
					return false;
				}
			}

			$this->db->commit();
			return true;
		} else {
			$this->output->add('user', false, $write);
			$this->db->rollback();
			return false;
		}
	}

	/**
	 * Sets the password for the given user
	 * @param array $user user object with
	 * 	'user_name' => name of the user
	 * 	'user_authdata' => password of the user (if null login is not possible)
	 * @param boolean $write True writes to output.
	 * @return boolean False on error.
	 */
	public function set_password(array $user, $write = true) {

		if (!$this->c->checkParameter($obj, ['user_name'], $write)) {
			return false;
		}

		if (!$this->auth->hasDelegatedUser($user['user_name']) && $auth->getUser() !== $user['user_name']) {
			$this->output->panic('403', 'Not allowed.', $write);
			return false;
		}

		if (is_null($user['user_authdata']) || $user['user_authdata'] === '') {
			$auth = null;
		} else {
			$auth = $this->create_password_hash(null, $user['user_authdata']);
		}

		$sql = "UPDATE users SET user_authdata = :user_authdata WHERE user_name = :user_name";

		$params = [
			':user_name' => $user['user_name'],
			':user_authdata' => $auth
		];

		return count($this->db->insert($sql, [$params])) > 0;
	}

	/**
	 * Deletes a user.
	 * @param array $obj object with
	 * 	'user_name' name of the user
	 * @output_flags delete is 'ok' when everything is fine, else 'not ok'
	 * @param boolean $write True writes to output.
	 * @return boolean false on error, else true
	 */
	public function delete(array $obj, $write = false) {
		if (!$this->c->checkParameter($obj, ['user_name'], $write)) {
			return false;
		}

		if (!$this->auth->hasDelegatedUser($obj['user_name']) && $auth->getUser() !== $obj['user_name']) {
			$this->output->add('403', 'Not allowed.', $write);
			return false;
		}

		$sql = "DELETE FROM users WHERE user_name = :username";

		$params = [':username' => $obj['user_name']];

		$status = $this->db->insert($sql, array($params));

		$this->output->addDebugMessage('delete', $status, $write);
		return count($status) > 0;
	}

	/**
	 * Updates the alias of the given user.
	 * @param array $obj object with
	 * 	'user_name' => name of the user
	 * 	'user_alias' => alias for this user.
	 * @param boolean $write True writes to output.
	 * @return boolean False on error.
	 */
	public function updateAlias(array $obj, $write = true) {
		if (!$this->c->checkParameter($obj, ['user_name', 'user_alias'], $write)) {
			return false;
		}

		if ($obj['user_name'] == $obj['user_alias']) {
			$this->output->panic('400', 'Cannot alias to the same user.', $write);
			return false;
		}

		if (!$this->auth->hasPermission('admin')) {
			$this->output->panic('403', 'Not allowed.', $write);
			return false;
		}

		$sql = "UPDATE users SET user_alias = :alias WHERE user_name = :user";

		$params = [
			':alias' => $obj['user_alias'],
			':user' => $obj['user_name']
		];

		return count($this->db->insert($sql, [$params])) > 0;
	}
}
