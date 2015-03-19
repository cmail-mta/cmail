<?php

	/**
	 * Class includes functions for handling addresses.
	 */
	class Address {


		private $db;
		private $output;

		public function __construct($db, $output) {
			$this->db = $db;
			$this->output = $output;
		}


		public function getAll() {


			$sql = "SELECT * FROM addresses ORDER BY address_order DESC";

			$out = $this->db->query($sql, array(), DB::F_ARRAY);

			$this->output->add("addresses", $out);

			return $out;
		}

		public function get($address) {


			if (!isset($address)) {
				$this->output->addDebugMessage("addresses", "We need an address.");
				return array();
			}

			$sql = "SELECT * FROM addresses WHERE address_expression = :address ORDER BY address_order DESC";

			$params = array(
				":address" => $address
			);

			$out = $this->db->query($sql, $params, DB::F_ARRAY);

			$this->output->add("addresses", $out);

			return $out;
		}

		public function getByUser($username) {


			if (!isset($username)) {
				$this->output->addDebugMessage("addresses", "We need an username.");
				return array();
			}

			$sql = "SELECT * FROM addresses WHERE address_user = :username ORDER BY address_order DESC";

			$params = array(
				":username" => $username
			);

			$out = $this->db->query($sql, $params, DB::F_ARRAY);

			$this->output->add("addresses", $out);

			return $out;
		}

		public function getNextOrder() {
			$sql = "SELECT address_order FROM addresses ORDER BY address_order DESC LIMIT 1";

			$out = $this->db->query($sql, array(), DB::F_ARRAY);
			
			if (count($out)) {

				return $out[0]["address_order"] + 1;
			}
			return 1;
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

			if (!isset($address["address_order"]) || $address["address_order"] == "") {
				$order = $this->getNextOrder();
				$this->output->addDebugMessage("address_order", "order set to " . $order);
			} else {
				$order = $address["address_order"];
			}

			$sql = "INSERT INTO addresses(address_expression, address_order, address_user) VALUES (:address_exp, :order, :username)";

			$params = array(
				":address_exp" => $address["address_expression"],
				":order" => $order,
				":username" => $address["address_user"]
			);

			$id = $this->db->insert($sql, array($params));

			if (is_null($id)) {
				$id = -1;
			}

			$this->output->addDebugMessage("addresses", "inserted row: " . $id);

			return $id;
		}

		public function update($address) {

			if (!isset($address["address_expression"])) {
				$this->output->addDebugMessage("addresses", "We need an address expression.");
				return false;
			}

			if (!isset($address["address_user"])) {
				$this->output->addDebugMessage("addresses", "We want also an username.");
				return false;
			}

			if (!isset($address["address_order"])) {
				$this->output->addDebugMessage("addresses", "We want also an order.");
				return false;
			}
			
			$sql = "UPDATE addresses SET address_order = :order, address_user = :username WHERE address_expression = :address_exp";

			$params = array(
				":address_exp" => $address["address_expression"],
				":username" => $address["address_user"],
				":order" => $address["address_order"]
			);

			$status = $this->db->insert($sql, array($params));

			if (isset($status)) {
				$this->output->add("status", "ok");
				return true;
			} else {
				$this->output->add("status", "not ok");
				return false;
			}

		}

		public function delete($expression) {
			
			if (!isset($expression)) {
				$this->output->add("status", "We need an address expression.");
				return false;
			}

			$sql = "DELETE FROM addresses WHERE address_expression = :expression";

			$params = array(
				":expression" => $expression
			);

			$status = $this->db->insert($sql, array($params));

			if (isset($status)) {
				$this->output->add("status", "ok");
				return true;
			} else {
				$this->output->add("status", "not ok");
				return false;
			}
		}


		public function switchOrder($address1, $address2) {

		}
	}

?>
