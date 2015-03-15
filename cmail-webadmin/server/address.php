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
	}

?>
