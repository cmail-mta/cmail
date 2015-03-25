<?php

	include_once("../module.php");

	/**
	 * Class includes functions for handling addresses.
	 */
	class Address implements Module {


		private $db;
		private $output;
		private $endPoints = array(
			"get" => "get",
			"add" => "add",
			"delete" => "delete",
			"update" => "update",
			"switch" => "switchOrder"
		);

		public function __construct($db, $output) {
			$this->db = $db;
			$this->output = $output;
		}


		public function getEndPoints() {
			return $this->endPoints;
		}

		public function isActive($username) {


			$obj = array("address_user" => $username);

			// if we have a row with this user
			return (count($this->getByUser($obj, false)) > 0);
		}

		public function get($obj, $write = true) {

			if (!isset($obj["address_expression"]) || empty($obj["address_expression"])) {

				if (isset($obj["address_user"]) && !empty($obj["address_user"])) {
					return $this->getByUser($obj);
				} else {
					return $this->getAll();
				}
			}

			$sql = "SELECT * FROM addresses WHERE address_expression = :address ORDER BY address_order DESC";

			$params = array(
				":address" => $obj["address_expression"]
			);

			$out = $this->db->query($sql, $params, DB::F_ARRAY);

			if ($write) {
				$this->output->add("addresses", $out);
			}

			return $out;
		}

		public function getAll() {


			$sql = "SELECT * FROM addresses ORDER BY address_order DESC";

			$out = $this->db->query($sql, array(), DB::F_ARRAY);

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

			$out = $this->db->query($sql, $params, DB::F_ARRAY);

			if ($write) {
				$this->output->add("addresses", $out);
			}

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
				return false;
			}

		}

		public function delete($obj) {
			
			if (!isset($obj["address_expression"])) {
				$this->output->add("status", "We need an address expression.");
				return false;
			}
			
			$sql = "DELETE FROM addresses WHERE address_expression = :expression";

			$params = array(
				":expression" => $obj["address_expression"]
			);

			$status = $this->db->insert($sql, array($params));

			if (isset($status)) {
				$this->output->add("status", "ok");
				return true;
			} else {
				return false;
			}
		}


		public function switchOrder($obj) {

			$this->db->beginTransaction();
			error_log(json_encode($obj));


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

			// swap orders
			$swap = $address1["address_order"];
			$address1["address_order"] = $address2["address_order"];
			$address2["address_order"] = $swap;

			if (!$this->delete($address1)) {
				$this->db->rollback();
				return false;
			}

			if(!$this->update($address2)) {
				$this->db->rollback();
				return false;
			}

			if (!$this->add($address1)) {
				$this->db->rollback();
				return false;
			}

			$this->db->commit();


			$this->output->add("status", "ok");
			return true;	
		}
	}

?>
