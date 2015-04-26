var cmail = {
	/**
	 * all routers
	 */
	inrouter: [
		"store",
		"forward",
		"handoff",
		"alias",
		"drop",
		"reject"
	],
	outrouter: [
		"drop",
		"any",
		"defined",
		"handoff",
		"alias",
		"reject"
	],
	modules: [],
	module: {
		get: function() {
			ajax.asyncGet(cmail.api_url + "?get_modules", function(xhr) {
				cmail.modules = JSON.parse(xhr.response).modules;
				var head = gui.elem("userlist_head");
				head.innerHTML = "";
				var tr = gui.create('tr');
				tr.appendChild(gui.createColumn("Name"));

				cmail.modules.forEach(function(module) {
					tr.appendChild(gui.createColumn(module));
				});
				tr.appendChild(gui.createColumn("Options"));
				head.appendChild(tr);
			});
		}
	},
	tabs: [
		"user",
		"delegates",
		"address",
		"msa",
		"pop",
		"test"
		],
	init: function() {
		this.module.get();

		// fill router checkboxes
		this.fill_router();

		// we need the user list for auto completion
		if (window.location.hash !== "#user") {
			this.user.get_all();
		}

		var self = this;

		// handle tab change
		this.switch_hash();
		window.addEventListener("hashchange", function() {
			self.switch_hash();
		}, false);
	},
	switch_hash: function() {
		var hash = window.location.hash;
		var test = true;
		this.user.hide_form();
		this.address.hide_form();
		var self = this;
		this.tabs.forEach(function(tab) {
			if ("#" + tab === hash) {
				gui.elem(tab).style.display = "block";
				test = false;
				if (tab != "test") {
					self[tab].get_all();
				}
			} else {
				gui.elem(tab).style.display = "none";
			}
		});

		// check for no tab is displayed
		if (test) {
			gui.elem(this.tabs[0]).style.display = "block";
		}
	},
	fill_router: function() {

		var inrouter = gui.elem("msa_inrouter");
		var outrouter = gui.elem("msa_outrouter");

		// fill inrouter
		this.inrouter.forEach(function(val) {
			inrouter.appendChild(gui.createOption(val, val));
		});

		// fill outrouter
		this.outrouter.forEach(function(val) {
			outrouter.appendChild(gui.createOption(val, val));
		});
	},
	set_status: function(message) {
		gui.elem("status").textContent = message;
	},
};
