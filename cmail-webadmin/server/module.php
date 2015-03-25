<?php

	$activeModules = array();

	function getModuleInstance($name, $db, $output) {
		global $activeModules, $modulelist;


		if (!isset($activeModules[$name])) {
			require_once($modulelist[$name]);
			$activeModules[$name] = new $name($db, $output);
		}
		return $activeModules[$name];
	}

	interface Module {
		public function getEndPoints();
		public function isActive($username);
	}

?>
