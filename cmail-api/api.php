<?php
/**
 * This file contains the basic api functions and the controller class.
 * @author Jan Düpmeier <j.duepmeier@googlemail.com>
 */

/**
 * current api version (must match the database version).
 */
$API_VERSION = 9;

require_once('config.php');
require_once('output.php');
require_once('pdo_sqlite.php');

// auth plugin
require_once('auth.php');

/**
 * This is the controller class for the api
 * It contains the basic 
 */
class Controller {

	/**
	 * output object.
	 */
	private $output;
	/**
	 * database object.
	 */
	private $db;
	/**
	 * authentication object.
	 */
	private $auth;

	/**
	 * Constructs a controller.
	 * @param DB $db database object
	 * @param Output $output output object
	 * @param Auth $auth authentication object
	 */
	public function __construct(DB $db, Output $output, Auth $auth) {

		$this->output = $output;
		$this->db = $db;
		$this->auth = $auth;
	}

	/**
	 * Returns the output object
	 * @return Output the output object
	 */
	public function getOutput() {
		return $this->output;
	}

	/**
	 * Returns the database object.
	 * @return DB database object.
	 */
	public function getDB() {
		return $this->db;
	}


	/**
	 * Checks with isset if the key exists.
	 * If not sends the error message.
	 * @param array $obj post object
	 * @param string[] $keys keys to check
	 * @param boolean $write True writes to output.
	 */
	public function checkParameter(array $obj, array $keys, $write = true) {
		foreach($keys as $key) {
			if (!isset($obj[$key])) {
				$this->output->panic('400', "{$key} is required.", $write);
				return false;
			}
		}
		return true;
	}
	/**
	 * Returns the authentication object.
	 * @return Auth authenticatoin object.
	 */
	public function getAuth() {
		return $this->auth;
	}

	/**
	 * Checks the database schema version against the version of this api.
	 * The api version is saved in the global variable $API_VERSION.
	 * @return Returns true when the version matches. False when they differ.
	 */
	public function checkSchemaVersion() {
		global $API_VERSION;

		$sql = "SELECT * FROM meta WHERE key = 'schema_version'";

		$out = $this->db->query($sql, [], DB::F_SINGLE_ASSOC);
		$this->output->add('api_version', $API_VERSION);
		$this->output->add('schema_version', $out['value']);

		return intval($out['value']) === $API_VERSION;
	}
}


/**
 * Main function call.
 * @param String name of the module with the entrypoint.
 * 	It can be NULL, if this is called from the api root path /.
 */
function main($module_name) {
	global $modulelist, $dbpath;
	error_reporting(E_ERROR);
	// init
	if(!session_start('cmail-api')) {
		die();
	}
	$output = Output::getInstance();
	$db = new DB($dbpath, $output);

	// db connection
	if (!$db->connect()) {
		$output->panic('500', 'Database Error!');
	}

	$auth = Auth::getInstance($db, $output);
	$c = new Controller($db, $output, $auth);

	if (!$c->checkSchemaVersion()) {
		$output->panic('500', 'Api version and schema version is not the same.');
		return;
	}

	// read post
	try {
		$http_raw = file_get_contents('php://input');
		$obj = json_decode($http_raw, true);
	} catch(Exception $e) {
		$output->panic('400', 'Cannot decode the payload.');
		return;
	}

	$output->addDebugMessage('payload', $obj);

	// check for auth data
	if (!isset($obj['auth'])) {
		$obj['auth'] = [];
	}


	if (!$auth->auth($obj['auth'])) {
		$output->add('login', false);
		$output->panic('401', 'Unauthorized.');
	}

	$output->add('login', true);
	$output->add('auth_user', $auth->getUser());

	// check for modules
	if (is_null($module_name)) {

		// get all current available modules
		if (isset($_GET['get_modules'])) {

			$modules = array();
			foreach ($modulelist as $name => $value) {
				$modules[] = $name;
			}

			$output->add('modules', $modules);
		}
	} else {
		// read modules
		$module = getModuleInstance($module_name, $c);
		foreach ($module->getEndPoints() as $ep => $func) {
			if (isset($_GET[$ep])) {
				$module->$func($obj);
				break;
			}
		}
	}

	$output->write();
}
