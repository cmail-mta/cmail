<?php
/**
 * This file contains the class DB.
 * @author Jan Düpmeier
 */


require_once('output.php');

/**
 * db wrapper class
 */
class DB {

	/**
	 * Flag for returning a single object from queries.
	 */
	const F_SINGLE_OBJECT = 2;
	/**
	 * Flag for returning a list of objects from queries.
	 */
	const F_OBJECT = 0;
	/**
	 * Flag for returning a array with rows from queries.
	 */
	const F_ARRAY = 1;
	/**
	 * Flag for returning a single row as assoc array from queries.
	 */
	const F_SINGLE_ASSOC = 3;

	/**
	 * The database object
	 */
	private $db;

	/**
	 * The order string.
	 */
	private $order = "";
	/**
	 * the output object.
	 */
	private $output;

	/**
	 * The path to database.
	 */
	private $dbpath = null;


	/**
	 * Basic constructor
	 * @param string $dbpath path to database.
	 * @param Output $output the output object.
	 */
	public function __construct($dbpath, Output $output) {
		$this->output = $output;
		$this->dbpath = $dbpath;
	}

	/**
	 * connect to database
	 * @return boolean False on error.
	 */
	function connect() {

		try {
			$this->db = new PDO("sqlite:{$this->dbpath}");
			$this->db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_WARNING);
		} catch (PDOException $e) {

			//header("Status: 500 " . $e->getMessage());
			//echo $e->getMessage();
			$this->output->add('status', $e->getMessage());
			return false;
		}

		$this->query('PRAGMA foreign_keys = ON', [], null);
		$this->query('PRAGMA busy_timeout = 1000', [], null);

		$this->output->addDebugMessage('db', 'connect call');
		return true;
	}

	/**
	 * Sets the order of select queries.
	 * @param string $tag set the column to order by.
	 * @param string $order order (ASC or DESC);
	 */
	function setOrder($tag, $order) {
		$this->order = " ORDER BY ${tag} ${order}";
	}

	/**
	 * Prepare an sql statement
	 * @param string sql The sql string.
	 * @return PDOStatement the sql statement
	 */
	function prepare($sql) {
		$stm = $this->db->prepare($sql);

		if ($this->db->errorCode() > 0) {
			$this->output->addDebugMessage('db', $this->db->errorInfo());
			return null;
		}
		return $stm;
	}

	/**
	 * Executes a prepared statement.
	 * @param PDOStatement $stm sql statement prepared prepare().
	 * @param array $params parameter for this prepared statement.
	 * @return PDOStatement Returns the statement.
	 */
	function execute(PDOStatement $stm, array $params) {
		$stm->execute($params);
		if ($this->db->errorCode() > 0) {
			$this->output->addDebugMessage('db', $this->db->errorInfo());
			//error_log($this->db->errorInfo());
			return null;
		}
		return $stm;
	}

	/**
	 * Queries an sql statement with the given params.
	 * @param string $sql The sql string
	 * @param array $params Array of parameter for the prepared statement.
	 * @param int $fetch The fetch format (see above in the class definition).
	 */
	function query($sql, $params, $fetch) {

		$this->output->addDebugMessage('dbquery', $sql);

		if (strpos($sql, 'SELECT') !== false) {

			$sql .= $this->order;

			if (isset($_GET['limit']) && !empty($_GET['limit'])) {
				$sql .= ' LIMIT :limit';
				$params[':limit'] = intval($_GET['limit']);
			}
		}

		$stm = $this->prepare($sql);

		if (is_null($stm)) {
			return null;
		}

		$stm->execute($params);

		if (is_null($stm)) {
			return null;
		}

		if (is_null($fetch)) {
			$stm->closeCursor();
			return null;
		}

		switch ($fetch) {
			case DB::F_SINGLE_OBJECT:
				$output = $stm->fetch(PDO::FETCH_OBJ);
				break;
			case DB::F_SINGLE_ASSOC:
				$output = $stm->fetch(PDO::FETCH_ASSOC);
				break;
			case DB::F_OBJECT:
		       		$output = $stm->fetchAll(PDO::FETCH_OBJ);
				break;
			case DB::F_ARRAY:
			default:
				$output = $stm->fetchAll(PDO::FETCH_ASSOC);
				break;
		}
		$stm->closeCursor();

		return $output;
	}

	/**
	 * insert objects into database
	 * @param sql sql insert statement
	 * @param params list of input data.
	 * @return array with the ids of the inserted items. Key is the number of the insert.
	 */
	function insert($sql, $params) {

		$this->output->addDebugMessage('dbquery', $sql);

		$stm = $this->db->prepare($sql);

		if ($this->db->errorCode() > 0) {
			$this->output->addDebugMessage('db', $this->db->errorInfo());
			return -1;
		}
		$ids = [];
		$insertNum = 0;

		foreach ($params as $param) {

			$stm->execute($param);

			if ($stm->errorInfo()[1] != 0) {
				$this->output->add('status', $stm->errorInfo()[2]);
				$stm->closeCursor();
				return $ids;
			}

			$ids[$insertNum] = $this->lastInsertID();
			$insertNum++;
		}

		$stm->closeCursor();

		return $ids;
	}

	/**
	 * Starts a transaction.
	 * @return True if no error occured.
	 */
	function beginTransaction() {
		if (!$this->db->beginTransaction()) {
			$this->output->addDebugMessage('transaction', $this->db->errorInfo());
			return false;
		}
		return true;
	}


	/**
	 * Commits a transaction.
	 * @return True if no error occured.
	 */
	function commit() {
		if (!$this->db->commit()) {
			$this->output->addDebugMessage('commit', $this->db->errorInfo());
			return false;
		}
		return true;
	}

	/**
	 * Rollback a transaction.
	 */
	function rollback() {
		$this->db->rollback();
	}

	/**
	 * Returns the last inserted id.
	 * @return The last iserted id
	 */
	function lastInsertID() {
		return $this->db->lastInsertId();
	}

}

