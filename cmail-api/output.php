<?php

/**
 * output functions
 */
class Output {

	private static $instance;
	private $retVal;
	private $headers;
	private $writed;

	/**
	 * constructor
	 */
	private function __construct() {
		global $debugFlag;
		$this->headers = [];
		$this->writed = false;
		if (isset($debugFlag)) {
			$this->debug = $debugFlag;
		} else {
			$this->debug = false;
		}
		$this->retVal['status'] = 'ok';
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
	 * @param string $key key to this array
	 * @param mixed $obj obj to add
	 * @param boolean $write True writes to array, false does nothing.
	 */
	public function add($key, $obj, $write = true) {
		$this->retVal[$key] = $obj;
	}

	/**
	 * set the error flag
	 */
	public function setErrorFlag() {
		$this->retVal['status'] = 'error';
	}

	/**
	 * Adds an status for output
	 * @param type $function problem function
	 * @param type $message message (use an array with 3 entries ("id", <code>, <message>))
	 * @param Boolean $error if this message is an error (default is false)
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
	 * If write is true: Adds an panic message and dies.
	 * @param string $statusCode HTTP status code
	 * @param string $message message to write in status
	 * @param boolean Flag for writing this message.
	 */
	public function panic($statusCode, $message, $write = true) {

		if (!$write) {
			return;
		}

		$this->headers[] = "HTTP/1.0 ${statusCode} ${message}";
		$this->add('status', $message);
		$this->write();
		die();
	}

	/**
	 * Generates the output for the browser. General you call this only once.
	 */
	public function write() {

		if ($this->writed) {
			return;
		}
		$this->writed = true;

		if (!$this->debug) {
			$this->retVal['debug'] = [];
		}

		$this->headers[] = 'Access-Control-Allow-Origin: *';
		$json = json_encode($this->retVal, JSON_NUMERIC_CHECK);
		# generate output
		if (isset($_GET['callback']) && !empty($_GET['callback'])) {
			$this->headers[] = 'Content-Type: application/javascript';
			$callback = $_GET['callback'];
			$output = "${callback}(${json});";
		} else {
			$this->headers[] = 'Content-Type: application/json';
			$output = $json;
		}
		header(implode('\r\n', $this->headers));
		echo $output;
	}
}
