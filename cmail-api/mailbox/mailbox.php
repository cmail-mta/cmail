<?php
	/**
	 * This file implements the Mailbox module.
	 * @author Jan Düpmeier <j.duepmeier@googlemail.com>
	 */
	require_once('../module.php');
	require_once('parser.php');

	/**
	 * Mailbox Module class
	 */
	class Mailbox implements Module {

		/**
		 * Database object.
		 */
		private $db;
		/**
		 * Controller object
		 */
		private $c;
		/**
		 * Path to user database.
		 */
		private $userdb = null;
		/**
		 * output object.
		 */
		private $output;
		/**
		 * user
		 */
		private $user;
		/**
		 * endpoints
		 */
		private $endPoints = [
			'get' => 'get',
			'read' => 'read',
			'unread' => 'unread',
			'delete' => 'delete'
		];

		/**
		 * Basic constructor
		 * @param Controller $c the controller object.
		 */
		public function __construct(Controller $c) {
			$this->c = $c;
			$this->db = $c->getDB();
			$this->output = $c->getOutput();

			$this->user = $c->getAuth()->getUser();

			$this->getUserDB();
		}

		/**
		 * This checks if a user database is provided and opens it.
		 * @param boolean $write True writes to output.
		 */
		public function getUserDB($write = true) {

			$sql = "SELECT user_database FROM users WHERE user_name = :user_name";

			$params = [
				':user_name' => $this->user
			];

			$msa = $this->db->query($sql, $params, DB::F_ARRAY);

			if (count($msa) < 1) {
				return;
			}

			$msa = $msa[0];

			if (!empty($msa['user_database'])) {
				$this->userdb = new DB($msa['user_database'], $this->output);
				if (!$this->userdb->connect()) {
					$this->output->add('warning', "Cannot open userdb from user: {$this->user}");
					$this->output->add('status', 'warning');
					$this->userdb = null;
				}
			}
		}

		/**
		 * Checks if the module is active for the given user.
		 * @param string $username username to check.
		 * @return True if module is active.
		 */
		public function isActive($username) {
			return count($this->get(['mail_user' => $username], false)) > 0;
		}

		/**
		 * Returns all endpoints for this module.
		 * @return array of endpoints.
		 */
		public function getEndPoints() {
			return $this->endPoints;
		}

		/**
		 * Returns all active users for this module.
		 * @return array of users.
		 */
		public function getActiveUsers() {
			$sql = "SELECT mail_user FROM mailbox GROUP BY mail_user";

			$users = $this->c->getDB()->query($sql, [], DB::F_ARRAY);

			$output = [];

			foreach($users as $user) {
				$output[$user['mail_user']] = true;
			}

			return $output;
		}

		/**
		 * Returns all mails with the requested settings and the right permissions.
		 * @param array $obj post object.
		 * @param boolean $write True writes to output.
		 * @return array list of mails.
		 */
		public function get(array $obj, $write = true) {

			$sql = "SELECT * FROM mailbox WHERE mail_user = :mail_user ORDER BY mail_submission DESC";

			$user = $this->user;

			if (!empty($obj['mail_user']) && !$write) {
				$user = $obj['mail_user'];
			}

			$params = [
				':mail_user' => $user
			];

			if (isset($obj['mail_only_unread'])) {
				$sql .= " AND mail_read = 0";
			}

			$out = $this->db->query($sql, $params, DB::F_ARRAY);
			if (!is_null($this->userdb)) {
				$out2 = $this->userdb->query($sql, params, DB::F_ARRAY);

				foreach($out2 as $mail) {
					$mail['mail_sourcedb'] = 'user';
					$out[] = $mail;
				}
			}

			if (!$write) {
				return $out;
			}

			$mails = [];
			foreach($out as $mail) {
				$mailBody = $mail['mail_data'];

				$parser = new MailParser($mailBody);
				$mail['mail_subject'] = $parser->getHeader('subject');
				$mail['mail_from'] = $parser->getHeader('from');
				$mail['mail_to'] = $parser->getHeader('to');
				$mail['mail_date'] = $parser->getHeader('date');
				$mail['mail_data'] = null;

				if (empty($mail['mail_sourcedb'])) {
					$mail['mail_sourcedb'] = 'master';
				}

				$mails[] = $mail;
			}

			$this->output->add('mails', $mails, $write);

			return $out;
		}

		/**
		 * Deletes a mail.
		 * @param array $obj post object
		 * @param $write True writes to output.
		 */
		public function delete(array $obj, $write = true) {
			if (empty($obj['mail_id'])) {
				$this->output->panic('400', 'mail_id is required.', $write);
				return false;
			}

			if (empty($obj['mail_sourcedb'])) {
				$source = 'master';
			} else {
				$source = $obj['mail_sourcedb'];
			}

			$sql = "DELETE FROM mailbox WHERE mail_id = :mail_id";

			$params = [
				':mail_id' => $obj['mail_id']
			];

			switch($source) {
				case 'user':
					if (!is_null($this->userdb)) {
						$status = $this->userdb->insert($sql, [$params]);
					}
					break;
				case 'master':
				default:
					$status = $this->db->insert($sql, [$params]);
					break;
			}

			if (count($status) > 0) {
				$this->output->add('status', 'ok', $write);
				return true;
			} else {
				return false;
			}
		}

		/**
		 * Sets the unread flag on the given mail.
		 * @param array $obj post object
		 * @param boolean $write True writes to output.
		 */
		public function unread(array $obj, $write = true) {

			if (!isset($obj['mail_id']) || empty($obj['mail_id'])) {
				$this->output->panic('400', 'mail_id is required.');
				return false;
			}

			if (empty($obj['mail_sourcedb'])) {
				$source = 'master';
			} else {
				$source = $obj['mail_sourcedb'];
			}

			$sql = "UPDATE mailbox SET mail_read = 0 WHERE mail_id = :mail_id";

			$params = [
				':mail_id' => $obj['mail_id']
			];

			switch($source) {
				case 'user':
					if (!is_null($this->userdb)) {
						$status = $this->userdb->insert($sql, [$params]);
					}
					break;
				case 'master':
				default:
					$status = $this->db->insert($sql, [$params]);
					break;
			}

			return count($status) > 0;
		}

		/**
		 * Returns a mail and sets the read flag on it.
		 * @param array $obj post object.
		 * @param boolean $write True writes to output.
		 */
		public function read(array $obj, $write = true) {

			if (!isset($obj['mail_id']) || empty($obj['mail_id'])) {
				$this->output->panic('400', 'mail_id is required.', $write);
				return null;
			}

			$sql = "SELECT * FROM mailbox WHERE mail_id = :mail_id";

			$params = [
				':mail_id' => $obj['mail_id']
			];

			$sql_read = "UPDATE mailbox SET mail_read = 1 WHERE mail_id = :mail_id";

			$source = '';

			if (!is_null($this->userdb)) {
				$out = $this->userdb->query($sql, $params, DB::F_ARRAY);

				if (count($out) < 1) {
					$out = $this->db->query($sql, $params, DB::F_ARRAY);

					if (count($out) <1 ) {
						$this->output->panic('400', 'Mail not found.', $write);
						return false;
					 } else {
						$source = 'master';
						$this->db->insert($sql_read, [$params]);
					 }
				} else {

					$source = 'user';
					$this->userdb->insert($sql_read, [$params]);
				}

			} else {
				$out = $this->db->query($sql, $params, DB::F_ARRAY);

				if (count($out) < 1) {
					$this->output->panic('400', 'Mail not found', $write);
					return false;
				}
				$source = 'master';
				$this->db->insert($sql_read, [$params]);
			}

			$out = $out[0];
			$out['mail_sourcedb'] = $source;

			$parser = new MailParser($out['mail_data']);

			$out['mail_from'] = $parser->getHeader('from');
			$out['mail_to'] = $parser->getHeader('to');
			$out['mail_subject'] = $parser->getHeader('subject');
			$out['mail_body'] = $parser->getBody();

			$this->output->add('mail', $out, $write);

			return $out;
		}
	}
?>
