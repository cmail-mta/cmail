<?php
	require_once("../module.php");
	require_once("MimeMailParser.class.php");

	class Mailbox implements Module {


		private $db;
		private $userdb = null;
		private $output;
		private $user;
		private $endPoints = array(
			"get" => "get",
			"read" => "read",
			"unread" => "unread",
			"delete" => "delete"
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
				$out2 = $this->userdb->query($sql, params, DB::F_ARRAY);
				
				foreach($out2 as $mail) {
					$mail["mail_sourcedb"] = "user";
					$out[] = $mail;
				}
			}

			$parser = new MimeMailParser();
			$mails = array();
			foreach($out as $mail) {
				$mailBody = $mail["mail_data"];
				
				$parser->setText($mailBody);
				$mail["mail_subject"] = $parser->getHeader("subject");
				$mail["mail_from"] = $parser->getHeader("from");
				$mail["mail_to"] = $parser->getHeader("to");
				$mail["mail_date"] = $parser->getHeader("date");
				$mail["mail_data"] = null;

				if (!isset($mail["mail_sourcedb"]) || empty($mail["mail_sourcedb"])) {
					$mail["mail_sourcedb"] = "master";
				}

				$mails[] = $mail;
			}

			if ($write) {
				$this->output->add("mails", $mails);
			}

			return $out;
		}

		public function delete($obj) {
			if (!isset($obj["mail_id"]) || empty($obj["mail_id"])) {
				$this->output->add("status", "No mail id is set");
				return false;
			}

			if (!isset($obj["mail_sourcedb"]) || empty($obj["mail_sourcedb"])) {
				$source = "master";
			} else {
				$source = $obj["mail_sourcedb"];
			}
			
			$sql = "DELETE FROM mailbox WHERE mail_id = :mail_id";

			$params = array(
				":mail_id" => $obj["mail_id"]
			);

			switch($source) {
				case "user":
					if (!is_null($this->userdb)) {
						$status = $this->userdb->insert($sql, [$params]);
					}
					break;
				case "master":
				default:
					$status = $this->db->insert($sql, [$params]);

			}

			if (isset($status)) {
				$this->output->add("status", "ok");
				return true;
			} else {
				return false;
			}
		}

		public function unread($obj) {


			if (!isset($obj["mail_id"]) || empty($obj["mail_id"])) {
				$this->output->add("status", "No mail id is set");
				return false;
			}

			if (!isset($obj["mail_sourcedb"]) || empty($obj["mail_sourcedb"])) {
				$source = "master";
			} else {
				$source = $obj["mail_sourcedb"];
			}

			$sql = "UPDATE mailbox SET mail_read = 0 WHERE mail_id = :mail_id";

			$params = array(
				":mail_id" => $obj["mail_id"]
			);

			switch($source) {
				case "user":
					if (!is_null($this->userdb)) {
						$this->userdb->insert($sql, [$params]);
					}
					break;
				case "master":
				default:
					$this->db->insert($sql, [$params]);
					break;
			}

		}

		public function read($obj) {

			if (!isset($obj["mail_id"]) || empty($obj["mail_id"])) {
				$this->output->add("status", "No mail id is set");
				return false;
			}

			$sql = "SELECT * FROM mailbox WHERE mail_id = :mail_id";

			$params = array(
				":mail_id" => $obj["mail_id"]
			);

			$sql_read = "UPDATE mailbox SET mail_read = 1 WHERE mail_id = :mail_id";

			$source = "";

			if (!is_null($this->userdb)) {
				$out = $this->userdb->query($sql, $params, DB::F_ARRAY);

				if (count($out) < 1) {
					$out = $this->db->query($sql, $params, DB::F_ARRAY);

					if (count($out) <1 ) {
						$this->output->add("status", "mail not found");
						return false;
					 } else {
						$source = "master";
						$this->db->insert($sql_read, [$params]);	
					 }
				} else {

					$source = "user";
					$this->userdb->insert($sql_read, [$params]);	
				}

			} else {
				$out = $this->db->query($sql, $params, DB::F_ARRAY);

				if (count($out) < 1) {
					$this->output->add("status", "mail not found");
					return false;
				}
				$source = "master";
				$this->db->insert($sql_read, [$params]);	
			}

			$out = $out[0];
			$out["mail_sourcedb"] = $source;
			$this->output->add("mail", $out);

			return $out;
		}
	}
?>
