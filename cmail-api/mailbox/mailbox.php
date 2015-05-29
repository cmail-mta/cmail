<?php
	require_once("../module.php");
	require_once("parser.php");

	class Mailbox implements Module {


		private $db;
		private $c;
		private $userdb = null;
		private $output;
		private $user;
		private $endPoints = array(
			"get" => "get",
			"read" => "read",
			"unread" => "unread",
			"delete" => "delete"
		);

		public function __construct($c) {
			$this->c = $c;
			$this->db = $c->getDB();
			$this->output = $c->getOutput();

			$this->user = $c->getAuth()->getUser();

			$this->getUserDB();
		}

		public function getUserDB() {


			$sql = "SELECT user_database FROM users WHERE user_name = :user_name";

			$params = array(
				":user_name" => $this->user
			);

			$msa = $this->db->query($sql, $params, DB::F_ARRAY);

			if (count($msa) < 1) {
				return;
			}

			$msa = $msa[0];

			if (isset($msa["user_database"]) && !empty($msa["user_database"]) && $msa["user_database"] != "") {
				$this->userdb = new DB($msa["user_database"], $this->output);
				if (!$this->userdb->connect()) {
					$this->output->add("warning", "Cannot open userdb from user: " . $this->user);
					$this->output->add("status", "warning");
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

		public function getActiveUsers() {
			$sql = "SELECT mail_user FROM mailbox GROUP BY mail_user";

			$users = $this->c->getDB()->query($sql, array(), DB::F_ARRAY);

			$output = array();

			foreach($users as $user) {
				$output[$user["mail_user"]] = true;
			}

			return $output;
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

			if (isset($obj["mail_only_unread"])) {
				$sql .= " AND mail_read = 0";
			}

			$out = $this->db->query($sql, $params, DB::F_ARRAY);
			if (!is_null($this->userdb)) {
				$out2 = $this->userdb->query($sql, params, DB::F_ARRAY);

				foreach($out2 as $mail) {
					$mail["mail_sourcedb"] = "user";
					$out[] = $mail;
				}
			}

			if (!$write) {
				return $out;
			}

			$mails = array();
			foreach($out as $mail) {
				$mailBody = $mail["mail_data"];

				$parser = new MailParser($mailBody);
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

			$parser = new MailParser($out["mail_data"]);

			$out["mail_from"] = $parser->getHeader("from");
			$out["mail_to"] = $parser->getHeader("to");
			$out["mail_subject"] = $parser->getHeader("subject");
			$out["mail_body"] = $parser->getBody();

			$this->output->add("mail", $out);

			return $out;
		}
	}
?>
