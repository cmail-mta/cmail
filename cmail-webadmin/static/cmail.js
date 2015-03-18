var cmail = {
	api_url: "server/api.php?",
	users: [],
	addresses: [],
	router: [
		"any",
		"none",
		"defined",
		"alias",
		"drop",
		"store",
		"forward",
		"handoff"
	],
	tabs: [
		"users",
		"addresses"
	],
	init: function() {
		this.get_users();
		this.fill_router();
		var self = this;
		this.switch_hash();
		window.addEventListener("hashchange", function() {
			self.switch_hash();	
		}, false);
	},
	get_user: function(name) {
		var xhr = ajax.syncPost(this.api_url + "get_user", JSON.stringify({ username: name}));
		var users = JSON.parse(xhr.response).users;

		return users[0];
	},
	switch_hash: function() {
		var hash = window.location.hash;
		var test = true;
		this.hide_user_form();
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
	get_users: function() {
		// getUsers
		var self = this;
		ajax.asyncGet(this.api_url + "get_users", function(xhr) {
			var users = JSON.parse(xhr.response).users;
			self.users = users;
			var userlist = gui.elem("userlist");

			users.forEach(function(user) {
				var tr = gui.create("tr");
				tr.appendChild(gui.createColumn(user.user_name));
				tr.appendChild(gui.createColumn(user.user_inrouter));
				tr.appendChild(gui.createColumn(user.user_inroute));
				tr.appendChild(gui.createColumn(user.user_outrouter));
				tr.appendChild(gui.createColumn(user.user_outroute));
				
				var options = gui.create("td");
				options.appendChild(gui.createButton("edit", self.show_user_form, [user.user_name], self));
				options.appendChild(gui.createButton("delete", self.delete_user, [user.user_name], self));
				tr.appendChild(options);	
				userlist.appendChild(tr);
			});
		});

	},
	fill_router: function() {
		
		var inrouter = gui.elem("user_inrouter");
		var outrouter = gui.elem("user_outrouter");

		this.router.forEach(function(val) {
			inrouter.appendChild(gui.createOption(val, val));
			outrouter.appendChild(gui.createOption(val, val));
		});
	},
	show_user_form: function(name) {
		
		if (name) {
			gui.elem("form_type").value = "edit";
			var user = this.get_user(name);

			if (!user) {
				alert("User not found!");
				return;
			}

			gui.elem("user_name").value = user.user_name;
			gui.elem("user_inroute").value = user.user_inroute;
			gui.elem("user_outroute").value = user.user_outroute;

			gui.elem("user_inrouter").value = user.user_inrouter;
			gui.elem("user_outrouter").value = user.user_outrouter;
		} else {

			gui.elem("form_type").value = "new";
			gui.elem("user_name").value = "";
			gui.elem("user_inroute").value = "";
			gui.elem("user_outroute").value = "";

		}

		gui.elem("useradd").style.display = "block";
	},
	hide_user_form: function() {
		gui.elem("useradd").style.display = "none";
	},
	edit_user: function() {

	}
};
