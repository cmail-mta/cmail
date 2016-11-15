<?php
	/**
	 * This file contains the interface for modules and a function to get Modules by names.
	 * @author Jan Düpmeier <j.duepmeier@googlemail.com>
	 */

	$activeModules = array();

	/**
	 * Return the module instance with the given name
	 * @param string $name name of the module.
	 * @param Controller $c controller object.
	 * @return Module module if found
	 */
	function getModuleInstance($name, Controller $c) {
		global $activeModules, $modulelist;

		if (!isset($activeModules[$name])) {
			// load module
			require_once($modulelist[$name]);
			$activeModules[$name] = new $name($c);
		}
		return $activeModules[$name];
	}

	/**
	 * Interface for modules.
	 * Every new Module must implement this interface.
	 */
	interface Module {
		/**
		 * Returns the endpoints of the module.
		 * @return array array of endpoints.
		 */
		public function getEndPoints();
		/**
		 * Checks if the given user is active in the module.
		 * @param string $username the username to check.
		 * @return boolean True if the user is active.
		 */
		public function isActive($username);
		/**
		 * Returns all active users for this module.
		 * @return array array of users.
		 */
		public function getActiveUsers();
	}

?>
