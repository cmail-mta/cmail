<?php

	include_once('../module.php');

	/**
	 * Class includes functions for handling addresses.
	 */
	class Address implements Module {


		private $output;
		private $c;

		// endpoints of this module
		private $endPoints = [
			'getRouter' => 'getRouter',
			'get' => 'get',
			'add' => 'add',
			'delete' => 'delete',
			'update' => 'update',
			'switch' => 'switchOrder',
			'test' => 'testRouting'
		];

		// default incoming router
		private $defaultInrouter = [
			'store' => true,
			'redirect' => true,
			'handoff' => true,
			'drop' => false,
			'reject' => false
		];

		/**
		 * Constructor for the address class.
		 * @param $c controller class
		 */
		public function __construct($c) {
			$this->c = $c;
			$this->output = $c->getOutput();
		}

		/**
		 * Returns all endpoints.
		 * @return object with endpoints as keys and
		 * 	   function names as value
		 */
		public function getEndPoints() {
			return $this->endPoints;
		}

		/**
		 * Returns all available incoming routers for this module
		 * @param array $obj post object. Not needed here.
		 * @param Boolean $write True means this method writes its result to the output object.
		 */
		public function getRouter(array $obj, $write = true) {

			// additional config routers
			global $addressInrouter;

			if (!isset($addressInrouter)) {
				$addressInrouter = [];
			}

			$inrouter = $addressInrouter + $this->defaultInrouter;

			$this->output->add('router', $inrouter, $write);

			return $inrouter;
		}

		/**
		 * Returns a list of all users who has an entry in the table.
		 * @return list with users as key and true as values
		 */
		public function getActiveUsers() {

			$sql = "SELECT address_route AS user FROM addresses WHERE address_router = 'store' GROUP BY address_route";

			$users = $this->c->getDB()->query($sql, [], DB::F_ARRAY);

			$output = [];
			foreach($users as $user) {

				$output[$user['user']] = true;
			}

			return $output;
		}

		/**
		 * Check if the username is active in this module.
		 * The address module is active for the user,
		 * when the user has at least one address(range).
		 * @param string $username username to check.
		 * @return True when the module is active.
		 */
		public function isActive($username) {

			$obj = array('address_user' => $username);

			// if we have a row with this user
			return (count($this->getByUser($obj, false)) > 0);
		}

		/**
		 * Check if the address is delegated.
		 * @param string $address address(range).
		 */
		private function checkAddressDelegate($address) {
			$auth = $this->c->getAuth();

			if (!$auth->isAuthorized()) {
				return false;
			}
			//check delegates
			if ($auth->hasPermission('admin')) {
				return true;
			}

			$sql = "SELECT * FROM api_address_delegates WHERE :address_expression LIKE api_expression AND api_user = :api_user";

			$params = array(
				':address_expression' => $address,
				':api_user' => $auth->getUser()
			);

			$out = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			return (count($out) > 0);

		}

		/**
		 * Check if the user is authorized for actions.
		 * @param string $user username
		 * @return True if the user can take actions.
		 */
		private function checkUserAction($user) {
			return $this->c->getAuth()->hasDelegatedUser($user);
		}

		/**
		 * Returns addresses from deletegated users.
		 * @return list of delegated user entries.
		 */
		private function getDelegatedUsers() {

			$sql = "SELECT * FROM addresses WHERE address_route = :address_route AND address_route = 'store'";

			$params = array(
				':address_route' => $this->c->getAuth()->getUser()
			);
			$list = $this->c->getAuth()->getDelegateUsers();
			foreach($list as $index => $item) {
				$sql .= " OR address_route = :address_route{$index}";
				$params[":address_route{$index}"] = $item;
			}

			$sql .= " ORDER BY address_order DESC";

			return $this->c->getDB()->query($sql, $params, DB::F_ARRAY);
		}


		/**
		 * Returns all delegated addresses to this user.
		 * @return address entries.
		 */
		private function getDelegatedAddresses() {

			$sql = "SELECT * FROM addresses WHERE :address_expression LIKE address_expression";

			$params = array();

			$list = $this->c->getAuth()->getDelegateAddresses();
			foreach($list as $index => $item) {
				$sql .= " OR :address_expression{$index} LIKE address_expression";
				$params[":address_expression{$index}"] = $item;
			}

			$sql .= " ORDER BY address_order DESC";

			$out = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			return $out;
		}

		/**
		 * Returns the address with the given order number.
		 * @param int $order the order number.
		 * @write boolean $write True when the address should write to output (Default is true).
		 * @return an array with the given order or an empty array.
		 */
		public function getByOrder($order, $write = true) {

			$sql = "SELECT * FROM addresses WHERE address_order = :order";

			$params = [
				':order' => $order
			];

			$out = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			if (count($out) > 0) {

				if (!$this->c->getAuth()->hasDelegatedAddress($out[0]['address_expression'])) {
					$this->output->panic('403', 'Forbidden.', $write);
					return [];
				}
			}

			$this->output->add('addresses', $out, $write);
			return $out;
		}

		/**
		 * Get the addresses.
		 * @param array $obj post object
		 * @param boolean $write True writes the array to the output object
		 * @return array of addresses
		 */
		public function get(array $obj, $write = true) {

			// specific address requested?
			if (isset($obj['address_order']) && !empty($obj['address_order'])) {

				return $this->getByOrder($obj['address_order'], $write);
			}

			// a address or address(range) requested?
			if (isset($obj['address_expression']) && !empty($obj['address_expression'])) {
				return $this->getByExpression($obj['address_expression'], $write);
			}

			if (isset($_GET['test'])) {
				return $this->getLikeExpression($_GET['test'], $write);
			}

			// get addresses by user?
			if (isset($obj['address_user']) && !empty($obj['address_user'])) {
				return $this->getByUser($obj);
			}

			// check permissions for all addresses
			$auth = $this->c->getAuth();

			if (!$auth->hasPermission('admin') && !$auth->hasPermission('delegate')) {
				return $this->getByUser(array('address_user' => $auth->getUser()));
			}

			if ($auth->hasPermission('delegate')) {
				return $this->getDelegated();
			}

			return $this->getAll($write);

		}


		/**
		 * Returns all addresses with matches the given expression.
		 * @param string $expression address expression
		 * @param boolean $write True writes to output.
		 * @return array with addresses.
		 */
		public function getLikeExpression($expression, $write = true) {

			$auth = $this->c->getAuth();
			if ($auth->hasPermission('delegate')) {
				$sql = "SELECT * FROM addresses
					WHERE (address_expression LIKE
					(SELECT api_expression FROM api_address_delegates
					WHERE api_address_delegates.api_user = :api_user)
					OR (address_router = 'store' AND address_route IN
					(SELECT api_delegate FROM api_user_delegates
					WHERE api_user_delegates.api_user = :api_user)))
						AND :address_expression LIKE address_expression ORDER BY address_order DESC";

				$params = [
					':address_expression' => $expression,
					':api_user' => $auth->getUser()
				];
			} else if ($auth->hasPermission('admin')) {
				$sql = "SELECT *
						FROM addresses
						WHERE :address LIKE address_expression
						ORDER BY address_order DESC";
				$params = [
					':address' => $expression
				];
			} else {
				$sql = "SELECT *
						FROM addresses
						WHERE :address LIKE address_expression
							AND address_route = 'store'
							AND address_route = :user
						ORDER BY address_order DESC";
				$params = [
					':address' => $expression,
					':user' => $auth->getUser()
				];
			}
			$addresses = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			$this->output->add('addresses', $addresses, $write);

			return $addresses;
		}

		/**
		 * Return addresses by expression.
		 * @param string $address expression
		 * @param boolean $write True writes to output object.
		 * @return array with addresses
		 */
		public function getByExpression($expression, $write = true) {

			$auth = $this->c->getAuth();
			if ($auth->hasPermission('delegate')) {
				$sql = "SELECT * FROM addresses
					WHERE (address_expression LIKE
					(SELECT api_expression FROM api_address_delegates
					WHERE api_address_delegates.api_user = :api_user)
					OR (address_router = 'store' AND address_route IN
					(SELECT api_delegate FROM api_user_delegates
					WHERE api_user_delegates.api_user = :api_user)))
				       	AND address_expression = :address_expression ORDER BY address_order DESC";

				$params = array(
					':address_expression' => $expression,
					':api_user' => $auth->getUser()
				);
			} else if ($auth->hasPermission('admin')) {
				$sql = "SELECT * FROM addresses WHERE address_expression = :address";
				$params = array(
					':address' => $expression
				);
			} else {
				$sql = "SELECT *
						FROM addresses
						WHERE address_expression = :address
							AND address_route = 'store'
							AND address_route = :user";
				$params = [
					':address' => $expression,
					':user' => $auth->getUser()
				];
			}

			$addresses = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			$this->output->add('addresses', $addresses, $write);

			return $addresses;
		}

		/**
		 * Returns all addresses in database.
		 * @return array with addresses
		 */
		public function getAll($write = true) {

			$sql = "SELECT * FROM addresses ORDER BY address_order DESC";

			$out = $this->c->getDB()->query($sql, [], DB::F_ARRAY);

			$this->output->add('addresses', $out, $write);

			return $out;
		}

		/**
		 * Return all addresses by user.
		 * @param array $obj post object
		 * @param boolean $write True writes to output.
		 */
		public function getByUser(array $obj, $write = true) {

			if (!isset($obj['address_user'])) {
				$this->output->panic('400', 'We need an username.');
				return [];
			}

			$sql = "SELECT *
					FROM addresses
					WHERE address_router = 'store'
						AND address_route = :username
					ORDER BY address_order DESC";

			$params = [
				':username' => $obj['address_user']
			];

			$out = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			$this->output->add('addresses', $out, $write);

			return $out;
		}


		// check if all required router arguments are there and valid.
		public function checkRouter($address, $write = true) {
			// config parameter for additional incoming router
			global $addressInrouter;

			if (!isset($address['address_router'])) {
				$this->output->panic('400', 'We cannot insert an address without an router.');
				return false;
			}

			$inrouter = $addressInrouter + $this->defaultInrouter;

			// check if router is in list
			if (!in_array($address['address_router'], $inrouter)) {
				$this->output->panic('400', 'Unkown inrouter.');
				return false;
			}
			// check if argument is required
			if ($inrouter[$address['address_router']] && !isset($address['address_route'])) {
				$this->output->panic('400', 'The address router needs an argument (address_route).');
				return false;
			}

			return true;

		}

		/**
		 * Adds an address(range) to database.
		 * @param array $address address object.
		 * @param boolean $write True writes to output.
		 * @return False on error.
		 */
		public function add(array $address, $write = true) {

			if (!isset($address['address_expression'])) {
				$this->output->panic('400', 'We cannot insert an address without an address expression.');
				return -3;
			}

			$auth = $this->c->getAuth();
			if (!$auth->hasPermission('admin') && !$auth->hasPermission('delegate')) {
				$this->output->panic('403', 'No permission to add an address.');
				return -4;
			}

			if ($auth->hasPermission('delegate')) {
				if (!$this->checkAddressDelegate($address['address_expression'])) {
					$this->output->panic('403', 'No permission to insert this expression.');
					return -5;
				}
			}

			if (!$this->checkRouter($address, $write)) {
				return -2;
			}

			$params = [
				':address_exp' => $address['address_expression'],
				':address_router' => $address['address_router'],
				':address_route' => isset($address['address_route']) ? $address['address_route'] : null
			];

			if (!isset($address['address_order']) || $address['address_order'] == '') {

				$sql = "INSERT INTO addresses(
							address_expression,
							address_router,
							address_route)
						VALUES (:address_exp, :address_router, :address_route)";
			} else {
				$params[':order'] = $address['address_order'];
				$sql = "INSERT INTO addresses(
							address_expression,
							address_order,
							address_router,
							address_route)
						VALUES (:address_exp, :order, :address_router, :address_route)";
			}

			$id = $this->c->getDB()->insert($sql, [$params]);

			if (count($id) > 0 && $id[0] > 0) {
				$this->output->addDebugMessage('addresses', "inserted row: {$id}", $write);
				$id = $id[0];
			} else {
				$id = -1;
			}

			return $id;
		}

		// checks the order
		private function checkOrder($old, $new) {

			// we can update entries with the same order.
			if ($old === $new) {
				return true;
			}

			// check if order is already in the database
			$sql = "SELECT address_order FROM addresses WHERE address_order = :order";

			$params = [
				':order' => $new
			];

			$addresses = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			return count($addresses) === 0;
		}

		// updates an entry in the database
		// the address object must contain this attributes:
		// - address_expression
		// - address_order
		// - address_old_order
		// - address_router
		// it must contain the attribute "address_route" if the router needs an argument.
		public function update($address, $write = true) {

			if (!isset($address['address_expression'])) {
				$this->output->panic('400', 'address_expression is required.');
				return false;
			}

			// this is needed because the order is the primary key
			if (!isset($address['address_old_order'])) {
				$this->output->panic('400', 'address_old_order is required.');
				return false;
			}

			if (!isset($address['address_order'])) {
				$this->output->panic('400', 'address_order is required.');
				return false;
			}

			$auth = $this->c->getAuth();
			if (!$auth->hasPermission('admin') && !$auth->hasPermission('delegate')) {
				$this->output->panic('403', 'No permission to update an address.');
				return false;
			}

			if ($auth->hasPermission('delegate') && !$this->checkAddressDelegate($address['address_expression'])) {
				$this->output->panic('403', 'No permission to update this to address');
				return false;
			}

			if (!$this->checkOrder($address['address_old_order'], $address['address_order'])) {
				$this->output->panic('400', 'Duplicate order.');
				return false;
			}

			if (!$this->checkRouter($address, $write)) {
				return false;
			}

			$sql = "UPDATE addresses
					SET address_expression = :address_exp,
						address_order = :order,
						address_router = :router,
						address_route = :route
					WHERE address_order = :old_order";

			$params = [
				':address_exp' => $address['address_expression'],
				':order' => $address['address_order'],
				':old_order' => $address['address_old_order'],
				':router' => $address['address_router'],
				':route' => isset($address['address_route']) ? $address['address_route'] : null
			];

			$status = $this->c->getDB()->insert($sql, [$params]);

			if (count($status) > 0) {
				$this->output->add('status', 'ok', $write);
				return true;
			} else {
				return false;
			}

		}

		/**
		 * Delete the given entry.
		 * @param array $obj post object
		 * @param boolean $write True writes to output.
		 * @return False if error.
		 */
		public function delete(array $obj, $write = true) {

			if (!isset($obj['address_expression'])) {
				$this->output->panic('400', 'address_expression is required.', $write);
				return false;
			}

			if (!isset($obj['address_order'])) {
				$this->output->panic('400', 'address_order is required.', $write);
				return false;
			}

			$auth = $this->c->getAuth();

			$test = false;
			if ($auth->hasPermission('admin')) {
				$test = true;
			} else if ($auth->hasPermission('delegate') && $this->checkAddressDelegate($obj['address_expression'])) {
				$test = true;
			}

			if (!$test) {
				$this->output->panic('403', 'Forbidden to delete this addresses.', $write);
				return false;
			}

			$sql = "DELETE FROM addresses WHERE address_order = :order";

			$params = [
				':order' => $obj['address_order']
			];

			$status = $this->c->getDB()->insert($sql, [$params]);

			if (count($status) > 0) {
				$this->output->add('status', 'ok', $write);
				return true;
			} else {
				return false;
			}
		}


		/**
		 * Switches the order of two addresses
		 * @param array $obj object with
		 * 	address1 address object 1
		 * 	address2 address object 2
		 * Address object has
		 * 	address_expression expression
		 * 	address_order current order
		 * 	@param boolean $write True writes to output object.
		 * 	@return False on error, else True.
		 */
		public function switchOrder(array $obj, $write = true) {

			if (!isset($obj['address1']) || empty($obj['address1'])) {
				$this->output->panic('400', 'address1 is required.', $write);
				return false;
			}

			if (!isset($obj['address2']) || empty($obj['address2'])) {
				$this->output->panic('status', 'address2 is required.', $write);
				return false;
			}

			$address1 = $obj['address1'];
			$address2 = $obj['address2'];

			if (!isset($address1['address_expression']) || empty($address1['address_expression'])) {
				$this->output->panic('400', 'Adress 1 has no address_expression', $write);
				return false;
			}


			if (!isset($address2['address_expression']) || empty($address2['address_expression'])) {
				$this->output->panic('400', 'Adress 2 has no address_expression', $write);
				return false;
			}

			if (!isset($address1['address_order']) && !isset($address2['address_order'])) {
				$this->output->panic('400', 'Both addresses needs an order number', $write);
				return false;
			}

			$auth = $this->c->getAuth();
			$test = false;
			if ($auth->hasPermission('admin')) {
				$test = true;
			} else if ($auth->hasPermission('delegate') && $this->checkAddressDelegate($obj['address_expression'])
					&& $this->checkAddressDelegate($address2['address_expression'])) {
				$test = true;
			}

			if (!$test) {
				$this->output->panic('403', 'Forbidden to swap this addresses.', $write);
				return false;
			}

			$this->c->getDB()->beginTransaction();

			// swap orders
			$swap = $address1['address_order'];
			$address1['address_order'] = $address2['address_order'];
			$address2['address_order'] = $swap;

			if (!$this->delete($address1)) {
				$this->c->getDB()->rollback();
				return false;
			}

			if(!$this->update($address2)) {
				$this->c->getDB()->rollback();
				return false;
			}

			if (!$this->add($address1)) {
				$this->c->getDB()->rollback();
				return false;
			}

			$this->c->getDB()->commit();

			$this->output->add('status', 'ok', $write);
			return true;
		}

		/**
		 * Return all addresses that matches the given expression
		 * @param array $obj object with
		 * 	address_expression expression for addresses
		 * @param boolean $write True writes to output object.
		 * @return list of addresses
		 */
		public function match(array $obj, $write = true) {

			if (!isset($obj['address_expression']) || empty($obj['address_expression'])) {
				$this->output->panic('400', 'address_expression is required.', $write);
				return [];
			}

			$sql = "SELECT address_expression,
						address_order,
						address_router,
						address_route
					FROM addresses
					WHERE :address_expression LIKE address_expression
					ORDER BY address_order DESC";

			$params = [
				':address_expression' => $obj['address_expression']
			];

			return $this->c->getDB()->query($sql, $params, DB::F_ARRAY);
		}

		/**
		 * Test a given routing
		 * @param array $obj post object
		 * @param boolean $write True writes to output.
		 * @return True if matched.
		 */
		// TODO: rewrite testRouting function!!!!
		public function testRouting($obj, $write = true) {

			if (!isset($obj['address_expression']) || empty($obj['address_expression'])) {
				$this->output->panic('400', 'address_expression is required.', $write);
				return false;
			}

			if (!isset($obj['address_routing']) || empty($obj['address_routing'])) {
				$this->output->panic('400', 'We need a routing direction (inrouter or outrouter)', $write);
				return false;
			}

			$address = $this->match($obj);

			if (count($address) < 1) {
				$this->output->add('steps', ['Address not found.'], $write);
				return false;
			}

			$address = $address[0];

			$router = "msa_{$obj['address_routing']}";

			$msaModule = getModuleInstance('SMTPD', $this->c);

			$finished = false;
			$steps = [];
			$steps[] = "Address matched: {$address['address_expression']}";
			$msa = $msaModule->get(['smtpd_user' => $address['address_route']], false);

			if (count($msa) < 1) {
				$this->output->add('steps', ["No routing informations found for user {$address['address_user']}"], $write);
				return false;
			}

			$msa = $msa[0];

			$steps[] = "Matched user: {$msa['msa_user']}";

			while (!$finished) {
				$finished = true;
				switch ($msa[$router]) {
					case 'drop':
						$steps[] = 'Mail dropped due to router';
						break;
					case 'handoff':
						$steps[] = "Handing off to {$msa[substr($router, 0, -1)]}";
						break;
					case 'forward':
						if ($router === 'msa_outrouter') {
							$steps[] = 'Not an outbound router (forward)';
						} else {
							$steps[] = "Forwarding to {$msa[substr($router, 0, -1)]}";
						}
						break;
					case 'reject':
						$steps[] = 'Address rejected';
						break;
					case 'store':
						if ($router === 'msa_outrouter') {
							$steps[] = 'Not an outbound router (store)';
							break;
						}

						if ($msa[substr($router, 0, -1)] != '') {
							$steps[] = 'Stored to database ' . $msa[substr($router, 0, -1)];
						} else {
							$steps[] = 'Stored to master database mailbox';
						}
						break;
					case 'any':
						if ($router == 'msa_outrouter') {
							$steps[] = 'Mail accepted (router any)';
						} else {
							$steps[] = 'Not an inbound router (any)';
						}
						break;
					case 'defined':
						if ($router == 'msa_outrouter') {
							$addresses = $this->get(array('address_user' => $msa['msa_user']));
							$accepted = false;
							foreach($addresses as $address2) {
								if ($obj['address_expression'] == $address2['address_expression']) {
									$steps[] = 'Address accepted';
									$accepted = true;
									break;
								}
							}
							if (!$accepted) {
								$steps[] = 'User can not user address (router defined for ' . $msa['msa_user'];
							}
						} else {
							$steps[] = 'Not in inbound router (defined)';
						}
						break;
					default:
						$steps[] = 'Unknown router ' . $msa[$router];
						break;
				}
			}
			$this->output->add('steps', $steps);
			return $steps;
		}
	}

?>
