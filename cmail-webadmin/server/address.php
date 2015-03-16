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


			$sql = "SELECT * FROM addresses";

			$out = $this->db->query($sql, array(), DB::F_ARRAY);

			$this->output->add("addresses", $out);

			return $out;
		}

		public function get($address) {


			if (!isset($address)) {
				$this->output->addDebugMessage("addresses", "We need an address.");
				return array();
			}

			$sql = "SELECT * FROM addresses WHERE address_expression = :address";

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

			$sql = "SELECT * FROM addresses WHERE address_user = :username";

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

		public function add($address_exp, $username, $order) {

			if (!isset($address_exp)) {
				$this->output->addDebugMessage("addresses", "We cannot insert an address without an address expression");
				return -3;
			}
			if (!isset($username)) {
				$this->output->addDebugMessage("addresses", "We cannot insert an address without an username");
				return -2;
			}

			if (!isset($order)) {
				$order = $this->getNextOrder();
				$this->output->addDebugMessage("address_order", "order set to " . $order);
			}

			$sql = "INSERT INTO addresses(address_expression, address_order, address_user) VALUES (:address_exp, :order, :username)";

			$params = array(
				":address_exp" => $address_exp,
				":order" => $order,
				":username" => $username
			);

			$id = $this->db->insert($sql, array($params));

			if (is_null($id)) {
				$id = -1;
			}

			$this->output->addDebugMessage("addresses", "inserted row: " . $id);

			return $id;
		}
	}

?>
