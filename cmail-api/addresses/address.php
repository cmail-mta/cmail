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

			$sql = "SELECT address_user FROM addresses GROUP BY address_user";

			$users = $this->c->getDB()->query($sql, array(), DB::F_ARRAY);

			$output = array();
			foreach($users as $user) {

				$output[$user["address_user"]] = true;
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
			if ($auth->hasRight("admin")) {
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

			if ($auth->hasRight("admin")) {
				return true;
			}

			if (!$auth->hasRight("delegate")) {
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

			$sql = "SELECT * FROM addresses WHERE address_user = :address_user";

			$params = array(
				":address_user" => $auth->getUser()
			);
			$list = $auth->getDelegateUsers();
			foreach($list as $index => $item) {
				$sql .= " OR address_user = :address_user" . $index;
				$params[":address_user" . $index] = $item;
			}

			$sql .= " ORDER BY address_order DESC";

			$out = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			return $out;
		}
		private function getDelegatedAddresses($auth) {

			$sql = "SELECT * FROM addresses WHERE :address_expression LIKE address_expression";

			$params = array(
				":address_user" => $auth->getUser()
			);
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


			$sql = "select * from addresses
				WHERE address_expression LIKE
				(select api_expression from api_address_delegates
				where api_address_delegates.api_user = :api_user)
				OR address_user IN
				(SELECT api_delegate FROM api_user_delegates
				WHERE api_user_delegates.api_user = :api_user) ORDER BY address_order DESC";

			$params = array(
				":api_user" => $auth->getUser()
			);

			$out = $this->c->getDB()->query($sql, $params, DB::F_ARRAY);

			if ($write) {
				$this->output->add("addresses", $out);
			}

			return $out;
		}

		public function get($obj, $write = true) {

			if (isset($obj["address_expression"]) && !empty($obj["address_expression"])) {
				return $this->getByExpression($obj["address_expression"], $write);
			}
			if (isset($obj["address_user"]) && !empty($obj["address_user"])) {
				return $this->getByUser($obj);
			}

			$auth = $this->c->getAuth();

			if (!$auth->hasRight("admin") && !$auth->hasRight("delegate")) {
				return $this->getByUser(array("address_user" => $auth->getUser()));
			}

			if ($auth->hasRight("delegate")) {
				return $this->getDelegated($auth);
			}

			return $this->getAll();

		}

		public function getByExpression($expression, $write = true) {

			$auth = $this->c->getAuth();
			if ($auth->hasRight("delegate")) {
				$sql = "select * from addresses
					WHERE (address_expression LIKE
					(select api_expression from api_address_delegates
					where api_address_delegates.api_user = :api_user)
					OR address_user IN
					(SELECT api_delegate FROM api_user_delegates
					WHERE api_user_delegates.api_user = :api_user))
				       	AND address_expression = :address_expression ORDER BY address_order DESC";


				$params = array(
					":address_expression" => $expression,
					":api_user" => $auth->getUser()
				);
			} else if ($auth->hasRight("admin")) {
				$sql = "SELECT * FROM addresses WHERE address_expression = :address";
				$params = array(
					":address" => $expression
				);
			} else {
				$sql = "SELECT * FROM addresses WHERE address_expression = :address AND address_user = :user";
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

			$sql = "SELECT * FROM addresses WHERE address_user = :username ORDER BY address_order DESC";

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
			if (!isset($address["address_user"])) {
				$this->output->addDebugMessage("addresses", "We cannot insert an address without an username");
				return -2;
			}

			$auth = $this->c->getAuth();
			if (!$auth->hasRight("admin") && !$auth->hasRight("delegate")) {
				$this->add("status", "No right to add an address.");
				return -4;
			}

			if ($auth->hasRight("delegate")) {
				if (!$this->checkAddressDelegate($address["address_expression"])) {
					$this->output->add("status", "No right to insert this expression.");
					return -5;
				}
			}

			$params = array(
				":address_exp" => $address["address_expression"],
				":username" => $address["address_user"]
			);

			if (!isset($address["address_order"]) || $address["address_order"] == "") {

				$sql = "INSERT INTO addresses(address_expression, address_user) VALUES (:address_exp, :username)";
			} else {
				$params[":order"] = $address["address_order"];
				$sql = "INSERT INTO addresses(address_expression, address_order, address_user) VALUES (:address_exp, :order, :username)";
			}

			$id = $this->c->getDB()->insert($sql, array($params));

			if (is_null($id)) {
				$id = -1;
			}

			$this->output->addDebugMessage("addresses", "inserted row: " . $id);

			return $id;
		}

		public function update($address) {

			if (!isset($address["address_expression"])) {
				$this->output->add("status", "We need an address expression.");
				return false;
			}

			if (!isset($address["address_user"])) {
				$this->output->add("status", "We want also an username.");
				return false;
			}

			if (!isset($address["address_order"])) {
				$this->output->add("status", "We want also an order.");
				return false;
			}

			$auth = $this->getAuth();
			if (!$auth->hasRight("admin") && !$auth->hasRight("delegate")) {
				$this->output->add("status", "No right to update an address.");
				return false;
			}

			if ($auth->hasRight("delegate") && !$this->checkAddressDelegate($address["address_expression"])) {
				$this->output->add("status", "No right to update this to address");
				return false;
			}

			$sql = "UPDATE addresses SET address_order = :order, address_user = :username WHERE address_expression = :address_exp";

			$params = array(
				":address_exp" => $address["address_expression"],
				":username" => $address["address_user"],
				":order" => $address["address_order"]
			);

			$status = $this->this->getDB()->insert($sql, array($params));

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

			$auth = $this->c->getAuth();

			$test = false;
			if ($auth->hasRight("admin")) {
				$test = true;
			} else if ($auth->hasRight("delegate") && $this->checkAddressDelegate($obj["address_expression"])) {
				$test = true;
			}

			if (!$test) {
				$this->output->add("status", "No access.");
				return false;
			}

			$sql = "DELETE FROM addresses WHERE address_expression = :expression";

			$params = array(
				":expression" => $obj["address_expression"]
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
			if ($auth->hasRight("admin")) {
				$test = true;
			} else if ($auth->hasRight("delegate") && $this->checkAddressDelegate($obj["address_expression"])
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

			$sql = "SELECT address_expression, address_user, address_order FROM addresses "
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

			$msaModule = getModuleInstance("MSA", $this->c);

			$finished = false;
			$steps = array();
			$steps[] = "Address matched: " . $address["address_expression"];
			$msa = $msaModule->get(array("msa_user" => $address["address_user"]), false);

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
						$steps[] = "address dropped";
						break;
					case "handoff":
						$steps[] = "handoff to " . $msa[substr($router, 0, -1)];
						break;
					case "forward":
						if ($router == "msa_outrouter") {
							$steps[] = "invalid outrouter (forward)";
						} else {
							$steps[] = "forward to " . $msa[substr($router, 0, -1)];
						}
						break;
					case "reject":
						$steps[] = "address rejected";
						break;
					case "store":
						if ($router == "msa_outrouter") {
							$steps[] = "store is not a valid outrouter";
							break;
						}

						if ($msa[substr($router, 0, -1)] != "") {
							$steps[] = "stored in " . $msa[substr($router, 0, -1)];
						} else {
							$steps[] = "stored in master table";
						}
						break;
					case "alias":
						$user = $msa[substr($router, 0, -1)];
						$finished = false;
						foreach($steps as $step) {
							if ($step == "alias to user " . $user) {
								$finished = true;
								$steps[] = "invalid routing, loop in aliases (user " . $user . ")";
								break;
							}
						}

						if (!$finished) {
							$msa = $msaModule->get(array("msa_user" => $user, false));

							if (count($msa) < 1) {
								$finished = true;
								$steps[] = "Cannot alias, user " . $user . " not found";
							} else {
								$msa = $msa[0];
								$steps[] = "alias to user " . $user;
							}
						}
						break;
					case "any":
						if ($router == "msa_outrouter") {
							$steps[] = "mail accepted (cause of any)";
						} else {
							$steps[] = "invalid routing (any is only valid for outrouting)";
						}
						break;
					case "defined":
						if ($router == "msa_outrouter") {
							$addresses = $this->get(array("address_user" => $msa["msa_user"]));
							$accepted = false;
							foreach($addresses as $address2) {
								if ($obj["address_expression"] == $address2["address_expression"]) {
									$steps[] = "address accepted";
									$accepted = true;
									break;
								}
							}
							if (!$accepted) {
								$steps[] = "address not accpeted (caused by defined routing with user " . $msa["msa_user"];
							}
						} else {
							$steps[] = "defined is only valid for outrouting";
						}
						break;
					default:
						$steps[] = "invalid routing (" . $msa[$router] . " not valid)";
						break;
				}
			}
			$this->output->add("steps", $steps);
			return $steps;
		}
	}

?>
