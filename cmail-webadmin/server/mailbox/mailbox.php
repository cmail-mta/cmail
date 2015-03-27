<?php
	require_once("../module.php");

	class Mailbox implements Module {


		private $db;
		private $userdb = null;
		private $output;
		private $user;
		private $endPoints = array(
			"get" => "get"	
		);

		public function __construct($db, $output) {
			$this->db = $db;
			$this->output = $output;

			$this->user = $_SERVER["PHP_AUTH_USER"];

			$this->getUserDB();
		}

		public function getUserDB() {

			$msaModule = getModuleInstance("MSA", $this->db, $this->output);

			$msa = $msaModule->get(array("msa_user" => $this->user), false);

			if (count($msa) < 1) {
				return;
			}

			$msa = $msa[0];

			if ($msa["msa_inrouter"] == "store" && isset($msa["msa_inroute"]) && !empty($msa["msa_inroute"])) {
				$this->userdb = new DB($this->output);
				if (!$this->userdb->connect()) {
					
					$this->userdb = null;
				}
			}

		}

		public function isActive($username) {
			
			return count($this->get(array("mail_user" => $username), false)) > 0;
		}

		public function getEndPoints() {
			return $this->endPoints;
		}

		public function get($obj, $write = true) {

			$sql = "SELECT * FROM mailbox WHERE mail_user = :mail_user";

			$user = $this->user;

			if (isset($obj["mail_user"]) && !empty($obj["mail_user"]) && !$write) {
				$user = $obj["mail_user"];
			}

			$params = array(
				":mail_user" => $user
			);

			$out = $this->db->query($sql, $params, DB::F_ARRAY);
			if (!is_null($this->userdb)) {
				$out2 = $this->db->query($sql, params, DB::F_ARRAY);
				
				foreach($out2 as $mail) {
					$out[] = $mail;
				}
			}
			if ($write) {
				$this->output->add("mails", $out);
			}

			return $out;
		}
	}
?>
