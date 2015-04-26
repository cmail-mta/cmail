var cmail = {
	/**
	 * all routers 
	 */
	tabs: [
		"list",
		"single",
		"write",
		],
	init: function() {

		this.mail.get_all();

		var self = this;

		// handle tab change
		this.switch_hash();
		window.addEventListener("hashchange", function() {
			self.switch_hash();	
		}, false);
	},
	switch_hash: function(hash) {
		if (!hash) { 
			hash = window.location.hash;
		}
		var test = true;
		var self = this;
		this.tabs.forEach(function(tab) {
			if ("#" + tab === hash) {
				gui.elem(tab).style.display = "block";
				test = false;
			} else {
				gui.elem(tab).style.display = "none";
			}
		});

		// check for no tab is displayed
		if (test) {
			gui.elem(this.tabs[0]).style.display = "block";
		}
	},
	set_status: function(message) {
		gui.elem("status").textContent = message;
	},
};
