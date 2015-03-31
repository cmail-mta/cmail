<?php

/**
 * output functions
 */
class Output {

	private static $instance;
	public $retVal;

	/**
	 * constructor
	 */
	private function __construct() {
		global $debugFlag;

		if (isset($debugFlag)) {
			$this->debug = $debugFlag;
		} else {
			$this->debug = false;
		}
		$this->retVal['status'] = "ok";
	}

	/**
	 * Returns the output instance or creates it.
	 * @return Output output instance
	 */
	public static function getInstance() {
		if (!self::$instance) {
			self::$instance = new self();
		}

		return self::$instance;
	}

	/**
	 * Adds data for use to output.
	 * @param type $table
	 * @param type $output
	 */
	public function add($table, $output) {
		$this->retVal[$table] = $output;
	}

	public function setErrorFlag() {
		$this->retVal["status"] = "error";
	}

	/**
	 * Adds an status for output
	 * @param type $function problem function
	 * @param type $message message (use an array with 3 entries ("id", <code>, <message>))
	 */
	public function addDebugMessage($function, $message, $error=false) {	

		if (isset($error) && $error) {
			$this->setErrorFlag();
		}

		if (!isset($this->retVal['debug'])) {
			$this->retVal['debug'] = [];
		}

		$this->retVal['debug'][$function][] = $message;
	}

	/**
	 * Generates the output for the browser. General you call this only once.
	 */
	public function write() {

		if (!$this->debug) {
			$this->retVal['debug'] = [];
		}

		header("Access-Control-Allow-Origin: *");
		# generate output
		if (isset($_GET["callback"]) && !empty($_GET["callback"])) {
			header("Content-Type: application/javascript");
			$callback = $_GET["callback"];
			echo $callback . "('" . json_encode($this->retVal, JSON_NUMERIC_CHECK) . "')";
		} else {
			header("Content-Type: application/json");
			echo json_encode($this->retVal, JSON_NUMERIC_CHECK);
		}
	}

}
?>
