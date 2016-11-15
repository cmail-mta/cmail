var cmail = {
	modules: [],
	module: {
		get: function() {
			api.get(cmail.api_url + "?get_modules", function(resp) {

				cmail.modules = resp.modules;
				gui.elem("auth_user").textContent = resp["auth_user"];
				var head = gui.elem("userlist_head");
				head.innerHTML = "";
				var tr = gui.create('tr');
				tr.appendChild(gui.createColumn("Name"));
				tr.appendChild(gui.createColumn("Alias"));
				tr.appendChild(gui.createColumn("Link count"));
				cmail.modules.forEach(function(module) {
					tr.appendChild(gui.createColumn(module));
				});

				tr.appendChild(gui.createColumn("Mail count"));

				tr.appendChild(gui.createColumn("Options"));
				head.appendChild(tr);
			});
		}
	},
	tabs: [
		"user",
		"delegates",
		"address",
		"smtpd",
		"pop"
	],
	init: function() {
		this.login();
	},
	login: function() {
		var auth = {
			user_name: gui.elem("login_name").value,
			password: gui.elem("login_password").value
		};

		api.post(cmail.api_url + "?login", JSON.stringify({ auth: auth }) , function(resp) {
			gui.elem("login_name").value = "";
			gui.elem("login_password").value = "";
			gui.elem('login_prompt').style.display = "none";
			cmail.main();
		});
	},
	main: function() {
		this.module.get();

		// fill router checkboxes
		this.get_router();

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
	logout: function() {
		api.get(cmail.api_url + "?logout", function(resp) {
			cmail.set_status(resp.status);
		});
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
	fill_router: function(router_elem, resp) {
		// fill list
		for (var key in resp.router) {
			if (!resp.router.hasOwnProperty(key)) {
				continue;
			}
			router_elem.appendChild(gui.createOption(key, key));
		}

		return resp.router;
	},
	get_router: function(elem) {
		api.get(cmail.api_url + "smtpd/?getRouter", function (resp) {
			this.inrouter = cmail.fill_router(gui.elem("smtpd_router"), resp);
		});

		api.get(cmail.api_url + "addresses/?getRouter", function (resp) {
			this.outrouter = cmail.fill_router(gui.elem("address_router"), resp);
		});
	},
	set_status: function(message) {
		gui.elem("status").textContent = message;
	},
};
