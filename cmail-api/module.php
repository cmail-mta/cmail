<?php

	$activeModules = array();

	function getModuleInstance($name, $c) {
		global $activeModules, $modulelist;


		if (!isset($activeModules[$name])) {
			require_once($modulelist[$name]);
			$activeModules[$name] = new $name($c);
		}
		return $activeModules[$name];
	}

	interface Module {
		public function getEndPoints();
		public function isActive($username);
		public function getActiveUsers();
	}

?>
