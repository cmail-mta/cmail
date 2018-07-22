<?php

	include_once("../module.php");

	/**
	 * Class includes functions for handling addresses.
	 */
	class Address implements Module {


		private $output;
		private $c;

		private $endPoints = array(
			"get" => "get",
			"add" => "add",
			"delete" => "delete",
			"update" => "update",
			"switch" => "switchOrder",
			"test" => "testRouting"
		);

		private $routersWithArgs = [
			"store",
			"redirect",
			"handoff"
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
		 * Returns a list of all users who has an entry in the table
		 * @return list with users as key and true as values
		 */
		public function getActiveUsers() {

			$sql = "SELECT address_route AS user FROM addresses WHERE address_router = 'store' GROUP BY address_route";

			$users = $this->c->getDB()->query($sql, array(), DB::F_ARRAY);

			$output = array();
			foreach($users as $user) {

				$output[$user["user"]] = true;
			}

			return $output;
		}

		public function isActive($username) {


			$obj = array("address_user" => $username);

			// if we have a row with this user
			return (count($this->getByUser($obj, false)) > 0);
		}


		private function checkAddressDelegate($address) {
			$auth = $this->c->getAuth();

			if (!$auth->isAuthorized()) {
				return false;
			}
			//check delegates
			if ($auth->hasPermission("admin")) {
				return true;
			}

			$sql = "SELECT * FROM api_address_delegates WHERE :address_expression LIKE api_expression AND api_user = :api_user";

			$params = array(
				":address_expression" => $address,
				":api_user" => $auth->getUser()
			);

			$out = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			return (count($out) > 0);

		}

		private function checkUserAction($user) {
			$auth = Auth::getInstance($this->c->getDB(), $this->output);

			if (!$auth->isAuthorized()) {
				return false;
			}

			if ($auth->hasPermission("admin")) {
				return true;
			}

			if (!$auth->hasPermission("delegate")) {
				return false;
			}

			if ($user == $auth->user) {
				return true;
			}

			$delegates = $auth->getDelegateUsers();

			foreach($delegates as $delegate) {
				if ($delegate === $user) {
					return true;
				}
			}

			return false;
		}

		private function getDelegatedUsers($auth) {

			$sql = "SELECT * FROM addresses WHERE address_route = :address_route AND address_route = 'store'";

			$params = array(
				":address_route" => $auth->getUser()
			);
			$list = $auth->getDelegateUsers();
			foreach($list as $index => $item) {
				$sql .= " OR address_route = :address_route" . $index;
				$params[":address_route" . $index] = $item;
			}

			$sql .= " ORDER BY address_order DESC";

			$out = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			return $out;
		}
		private function getDelegatedAddresses($auth) {

			$sql = "SELECT * FROM addresses WHERE :address_expression LIKE address_expression";

			$params = array();

			$list = $auth->getDelegateAddresses();
			foreach($list as $index => $item) {
				$sql .= " OR :address_expression" . $index . " LIKE address_expression";
				$params[":address_expression" . $index] = $item;
			}

			$sql .= " ORDER BY address_order DESC";

			$out = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			return $out;
		}

		private function GetDelegated($auth, $write = true) {


			$sql = "SELECT * FROM addresses
				WHERE address_expression LIKE
				(SELECT api_expression FROM api_address_delegates
				WHERE api_address_delegates.api_user = :api_user)
				OR (address_router = 'store' AND address_route IN
				(SELECT api_delegate FROM api_user_delegates
				WHERE api_user_delegates.api_user = :api_user)) ORDER BY address_order DESC";

			$params = array(
				":api_user" => $auth->getUser()
			);

			$out = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			if ($write) {
				$this->output->add("addresses", $out);
			}

			return $out;
		}

		public function getByOrder($order, $write = true) {

			$sql = "SELECT * FROM addresses WHERE address_order = :order";

			$params = array(
				":order" => $order
			);

			$out = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			if (count($out) > 0) {

				if (!$this->c->getAuth()->hasDelegatedAddress($out[0]["address_expression"])) {
					$this->output->add("status", "Not authorized.");
					return [];
				}

				$this->output->add("addresses", $out);
				return $out;
			} else {
				$this->output->add("addresses", []);
				return [];
			}
		}

		public function get($obj, $write = true) {

			if (isset($obj["address_order"]) && !empty($obj["address_order"])) {

				return $this->getByOrder($obj["address_order"], $write);
			}

			if (isset($obj["address_expression"]) && !empty($obj["address_expression"])) {
				return $this->getByExpression($obj["address_expression"], $write);
			}

			if (isset($_GET["test"])) {
				return $this->getLikeExpression($_GET["test"], $write);
			}

			if (isset($obj["address_user"]) && !empty($obj["address_user"])) {
				return $this->getByUser($obj);
			}

			$auth = $this->c->getAuth();

			if (!$auth->hasPermission("admin") && !$auth->hasPermission("delegate")) {
				return $this->getByUser(array("address_user" => $auth->getUser()));
			}

			if ($auth->hasPermission("delegate")) {
				return $this->getDelegated($auth);
			}

			return $this->getAll();

		}

		public function getLikeExpression($expression, $write = true) {

			$auth = $this->c->getAuth();
			if ($auth->hasPermission("delegate")) {
				$sql = "SELECT * FROM addresses
					WHERE (address_expression LIKE
					(SELECT api_expression FROM api_address_delegates
					WHERE api_address_delegates.api_user = :api_user)
					OR (address_router = 'store' AND address_route IN
					(SELECT api_delegate FROM api_user_delegates
					WHERE api_user_delegates.api_user = :api_user)))
						AND :address_expression LIKE address_expression ORDER BY address_order DESC";


				$params = array(
					":address_expression" => $expression,
					":api_user" => $auth->getUser()
				);
			} else if ($auth->hasPermission("admin")) {
				$sql = "SELECT * FROM addresses WHERE :address LIKE address_expression ORDER BY address_order DESC";
				$params = array(
					":address" => $expression
				);
			} else {
				$sql = "SELECT * FROM addresses WHERE :address LIKE address_expression AND address_route = 'store' AND address_route = :user ORDER BY address_order DESC";
				$params = array(
					":address" => $expression,
					":user" => $auth->getUser()
				);
			}
			$addresses = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			if ($write) {
				$this->output->add("addresses", $addresses);
			}

			return $addresses;
		}

		public function getByExpression($expression, $write = true) {

			$auth = $this->c->getAuth();
			if ($auth->hasPermission("delegate")) {
				$sql = "SELECT * FROM addresses
					WHERE (address_expression LIKE
					(SELECT api_expression FROM api_address_delegates
					WHERE api_address_delegates.api_user = :api_user)
					OR (address_router = 'store' AND address_route IN
					(SELECT api_delegate FROM api_user_delegates
					WHERE api_user_delegates.api_user = :api_user)))
				       	AND address_expression = :address_expression ORDER BY address_order DESC";


				$params = array(
					":address_expression" => $expression,
					":api_user" => $auth->getUser()
				);
			} else if ($auth->hasPermission("admin")) {
				$sql = "SELECT * FROM addresses WHERE address_expression = :address";
				$params = array(
					":address" => $expression
				);
			} else {
				$sql = "SELECT * FROM addresses WHERE address_expression = :address AND address_route = 'store' AND address_route = :user";
				$params = array(
					":address" => $expression,
					":user" => $auth->getUser()
				);
			}

			$addresses = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			if ($write) {
				$this->output->add("addresses", $addresses);
			}

			return $addresses;
		}

		public function getAll() {


			$sql = "SELECT * FROM addresses ORDER BY address_order DESC";

			$out = $this->c->getDB()->query($sql, array(), DB::F_ARRAY);

			$this->output->add("addresses", $out);

			return $out;
		}

		public function getByUser($obj, $write = true) {


			if (!isset($obj["address_user"])) {
				$this->output->addDebugMessage("addresses", "We need an username.");
				return array();
			}

			$sql = "SELECT * FROM addresses WHERE address_router = 'store' AND address_route = :username ORDER BY address_order DESC";

			$params = array(
				":username" => $obj["address_user"]
			);

			$out = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			if ($write) {
				$this->output->add("addresses", $out);
			}

			return $out;
		}

		public function add($address) {

			if (!isset($address["address_expression"])) {
				$this->output->addDebugMessage("addresses", "We cannot insert an address without an address expression");
				return -3;
			}
			if (!isset($address["address_router"])) {
				$this->output->addDebugMessage("addresses", "We cannot insert an address without an router");
				return -2;
			}
			if (in_array($address["address_router"], $this->routersWithArgs) && !isset($address["address_router"])) {
				$this->output->addDebugMessage("addresses", "The address router needs an argument (address_route)");
				return -2;
			}
			$auth = $this->c->getAuth();
			if (!$auth->hasPermission("admin") && !$auth->hasPermission("delegate")) {
				$this->add("status", "No permission to add an address.");
				return -4;
			}

			if ($auth->hasPermission("delegate")) {
				if (!$this->checkAddressDelegate($address["address_expression"])) {
					$this->output->add("status", "No permission to insert this expression.");
					return -5;
				}
			}

			$params = array(
				":address_exp" => $address["address_expression"],
				":address_router" => $address["address_router"],
				":address_route" => isset($address["address_route"]) ? $address["address_route"] : null
			);

			if (!isset($address["address_order"]) || $address["address_order"] == "") {

				$sql = "INSERT INTO addresses(address_expression, address_router, address_route) VALUES (:address_exp, :address_router, :address_route)";
			} else {
				$params[":order"] = $address["address_order"];
				$sql = "INSERT INTO addresses(address_expression, address_order, address_router, address_route) VALUES (:address_exp, :order, :address_router, :address_route)";
			}

			$id = $this->c->getDB()->insert($sql, array($params));

			if (is_null($id)) {
				$id = -1;
			}

			$this->output->addDebugMessage("addresses", "inserted row: " . $id);

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
				":order" => $new
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
		public function update($address) {

			if (!isset($address["address_expression"])) {
				$this->output->add("status", "We need an address expression.");
				return false;
			}

			// this is needed because the order is the primary key
			if (!isset($address["address_old_order"])) {
				$this->output->add("status", "We want also the old order");
				return false;
			}

			if (!isset($address["address_order"])) {
				$this->output->add("status", "We want also an order.");
				return false;
			}

			$auth = $this->c->getAuth();
			if (!$auth->hasPermission("admin") && !$auth->hasPermission("delegate")) {
				$this->output->add("status", "No permission to update an address.");
				return false;
			}

			if ($auth->hasPermission("delegate") && !$this->checkAddressDelegate($address["address_expression"])) {
				$this->output->add("status", "No permission to update this to address");
				return false;
			}

			if (!isset($address["address_router"])) {
				$this->output->add("status", "No address router defined.");
				return false;
			}

			if (!$this->checkOrder($address["address_old_order"], $address["address_order"])) {
				$this->output->add("status", "Duplicate order.");
				return false;
			}

			if (in_array($address["address_router"], $this->routersWithArgs) && !isset($address["address_route"])) {
				$this->output->add("status", "Address router needs an argument (address_route).");
				return false;
			}

			$sql = "UPDATE addresses SET address_expression = :address_exp, address_order = :order, address_router = :router, address_route = :route  WHERE address_order = :old_order";

			$params = array(
				":address_exp" => $address["address_expression"],
				":order" => $address["address_order"],
				":old_order" => $address["address_old_order"],
				":router" => $address["address_router"],
				":route" => isset($address["address_route"]) ? $address["address_route"] : null
			);

			$status = $this->c->getDB()->insert($sql, array($params));

			if (isset($status)) {
				$this->output->add("status", "ok");
				return true;
			} else {
				return false;
			}

		}

		public function delete($obj) {

			if (!isset($obj["address_expression"])) {
				$this->output->add("status", "We need an address expression.");
				return false;
			}

			if (!isset($obj["address_order"])) {
				$this->output->add("status", "We need the address order.");
				return false;
			}

			$auth = $this->c->getAuth();

			$test = false;
			if ($auth->hasPermission("admin")) {
				$test = true;
			} else if ($auth->hasPermission("delegate") && $this->checkAddressDelegate($obj["address_expression"])) {
				$test = true;
			}

			if (!$test) {
				$this->output->add("status", "No access.");
				return false;
			}

			$sql = "DELETE FROM addresses WHERE address_order = :order";

			$params = array(
				":order" => $obj["address_order"]
			);

			$status = $this->c->getDB()->insert($sql, array($params));

			if (isset($status)) {
				$this->output->add("status", "ok");
				return true;
			} else {
				return false;
			}
		}


		/**
		 * Switches the order of two addresses
		 * @param $obj object with
		 * 	address1 address object 1
		 * 	address2 address object 2
		 * Address object has
		 * 	address_expression expression
		 * 	address_order current order
		 */
		public function switchOrder($obj) {

			if (!isset($obj["address1"]) || empty($obj["address1"])) {
				$this->output->add("status", "Adress 1 is not defined");
				return false;
			}

			if (!isset($obj["address2"]) || empty($obj["address2"])) {
				$this->output->add("status", "Address 2 is not defined");
				return false;
			}

			$address1 = $obj["address1"];
			$address2 = $obj["address2"];

			if (!isset($address1["address_expression"]) || empty($address1["address_expression"])) {
				$this->output->add("status", "Adress 1 has no address expression");
				return false;
			}


			if (!isset($address2["address_expression"]) || empty($address2["address_expression"])) {
				$this->output->add("status", "Adress 2 has no address expression");
				return false;
			}

			if (!isset($address1["address_order"]) && !isset($address2["address_order"])) {
				$this->output->add("status", "Both addresses needs an order number");
				return false;
			}

			$auth = $this->c->getAuth();
			$test = false;
			if ($auth->hasPermission("admin")) {
				$test = true;
			} else if ($auth->hasPermission("delegate") && $this->checkAddressDelegate($obj["address_expression"])
					&& $this->checkAddressDelegate($address2["address_expression"])) {
				$test = true;
			}
			$this->c->getDB()->beginTransaction();

			// swap orders
			$swap = $address1["address_order"];
			$address1["address_order"] = $address2["address_order"];
			$address2["address_order"] = $swap;

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


			$this->output->add("status", "ok");
			return true;
		}

		/**
		 * Return all addresses that matches the given expression
		 * @param $obj object with
		 * 	address_expression expression for addresses
		 * @return list of addresses
		 */
		public function match($obj) {


			if (!isset($obj["address_expression"]) || empty($obj["address_expression"])) {
				$this->output->add("status", "We need an address expression");
				return [];
			}

			$sql = "SELECT address_expression, address_order, address_router, address_route FROM addresses "
				. "WHERE :address_expression LIKE address_expression ORDER BY address_order DESC";

			$params = array(
				":address_expression" => $obj["address_expression"]
			);

			$out = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			return $out;

		}

		/**
		 * Test a given routing
		 */
		// TODO: rewrite testRouting function!!!!
		public function testRouting($obj) {

			if (!isset($obj["address_expression"]) || empty($obj["address_expression"])) {
				$this->output->add("status", "We need an address expression.");
				return false;
			}

			if (!isset($obj["address_routing"]) || empty($obj["address_routing"])) {
				$this->output->add("status", "We need a routing direction (inrouter or outrouter)");
				return false;
			}

			$address = $this->match($obj);

			if (count($address) < 1) {
				$this->output->add("steps", ["Address not found."]);
				return false;
			}

			$address = $address[0];

			$router = "msa_" . $obj["address_routing"];

			$msaModule = getModuleInstance("SMTPD", $this->c);

			$finished = false;
			$steps = array();
			$steps[] = "Address matched: " . $address["address_expression"];
			$msa = $msaModule->get(array("smtpd_user" => $address["address_route"]), false);

			if (count($msa) < 1) {
				$this->output->add("steps", ["No routing informations found for user " . $address["address_user"]]);
				return false;
			}

			$msa = $msa[0];

			$steps[] = "Matched user: " . $msa["msa_user"];

			while (!$finished) {
				$finished = true;
				switch ($msa[$router]) {
					case "drop":
						$steps[] = "Mail dropped due to router";
						break;
					case "handoff":
						$steps[] = "Handing off to " . $msa[substr($router, 0, -1)];
						break;
					case "forward":
						if ($router == "msa_outrouter") {
							$steps[] = "Not an outbound router (forward)";
						} else {
							$steps[] = "Forwarding to " . $msa[substr($router, 0, -1)];
						}
						break;
					case "reject":
						$steps[] = "Address rejected";
						break;
					case "store":
						if ($router == "msa_outrouter") {
							$steps[] = "Not an outbound router (store)";
							break;
						}

						if ($msa[substr($router, 0, -1)] != "") {
							$steps[] = "Stored to database " . $msa[substr($router, 0, -1)];
						} else {
							$steps[] = "Stored to master database mailbox";
						}
						break;
					case "any":
						if ($router == "msa_outrouter") {
							$steps[] = "Mail accepted (router any)";
						} else {
							$steps[] = "Not an inbound router (any)";
						}
						break;
					case "defined":
						if ($router == "msa_outrouter") {
							$addresses = $this->get(array("address_user" => $msa["msa_user"]));
							$accepted = false;
							foreach($addresses as $address2) {
								if ($obj["address_expression"] == $address2["address_expression"]) {
									$steps[] = "Address accepted";
									$accepted = true;
									break;
								}
							}
							if (!$accepted) {
								$steps[] = "User can not user address (router defined for " . $msa["msa_user"];
							}
						} else {
							$steps[] = "Not in inbound router (defined)";
						}
						break;
					default:
						$steps[] = "Unknown router " . $msa[$router];
						break;
				}
			}
			$this->output->add("steps", $steps);
			return $steps;
		}
	}

?>
