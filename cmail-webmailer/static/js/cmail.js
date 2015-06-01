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
		this.login();
	},
	login: function() {
		var auth = {
                        user_name: gui.elem("login_name").value,
                        password: gui.elem("login_password").value
                };

                ajax.asyncPost(cmail.api_url + "?login", JSON.stringify({ auth: auth }) , function(xhr) {
                        var resp = JSON.parse(xhr.response);

                        cmail.set_status(resp.status);
                        gui.elem("login_name").value = "";
                        gui.elem("login_password").value = "";

                        // check for correct login
                        if (!resp.login) {
                                gui.elem("login_prompt").style.display = "block";
                        } else {
                                gui.elem("login_prompt").style.display = "none";
                                cmail.main();
                        }
                });
	},
	main: function() {

		this.mail.get_all();

		var self = this;

		// handle tab change
		this.switch_hash();
		window.addEventListener("hashchange", function() {
			self.switch_hash();
		}, false);
	},
	logout: function() {
                ajax.asyncGet(cmail.api_url + "?logout", function(xhr) {
                        cmail.set_status(JSON.parse(xhr.response).status);
                });
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
