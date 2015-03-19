var cmail = {
	api_url: "server/api.php?",
	users: [],
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
		this.get_addresses();
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
	get_address: function(expression) {
		var xhr = ajax.syncPost(this.api_url + "get_address", JSON.stringify({ address: expression}));
		var addresses = JSON.parse(xhr.response).addresses;

		return addresses[0];
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
			userlist.innerHTML = "";

			var list = {};
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

				list[user.user_name] = user.user_name;
			});

			self.fill_username_list(list);
		});

	},
	fill_username_list: function(list) {
		var userlist = gui.elem("usernamelist");
		userlist.innerHTML = "";
		Object.keys(list).forEach(function(username) {
			userlist.appendChild(gui.createOption("", username));
		});
	},
	get_addresses: function() {
		var self = this;

		ajax.asyncGet(this.api_url + "get_addresses", function(xhr) {

			var addresses = JSON.parse(xhr.response).addresses;

			var addresslist = gui.elem("addresslist");

			addresslist.innerHTML = "";

			addresses.forEach(function(address) {

				var tr = gui.create("tr");

				tr.appendChild(gui.createColumn(address.address_expression));
				tr.appendChild(gui.createColumn(address.address_order));
				tr.appendChild(gui.createColumn(address.address_user));

				var options = gui.create("td");
				options.appendChild(gui.createButton("edit", self.show_address_form, [address.address_expression], self));
				options.appendChild(gui.createButton("delete", self.delete_address, [address.address_expression], self));
				tr.appendChild(options);
				addresslist.appendChild(tr);
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
			gui.elem("user_name").disabled = true;
			gui.elem("user_inroute").value = user.user_inroute;
			gui.elem("user_outroute").value = user.user_outroute;

			gui.elem("user_inrouter").value = user.user_inrouter;
			gui.elem("user_outrouter").value = user.user_outrouter;

		} else {

			gui.elem("form_type").value = "new";
			gui.elem("user_name").value = "";
			gui.elem("user_name").disabled = false;
			gui.elem("user_inroute").value = "";
			gui.elem("user_outroute").value = "";

		}

		gui.elem("user_password").value = "";
		gui.elem("user_password2").value = "";

		gui.elem("useradd").style.display = "block";
	},
	show_address_form: function(expression) {
		if (expression) {
			gui.elem("form_type").value = "edit";

			var address = this.get_address(expression);

			if (!address) {
				alert("Address not found!");
				return;
			}

			gui.elem("address_expression").value = address.address_expression;
			gui.elem("address_expression").disabled = true;
			gui.elem("address_order").value = address.address_order;
			gui.elem("address_user").value = address.address_user;
		} else {
			gui.elem("form_address_type").value = "new";
			gui.elem("address_expression").value = "";
			gui.elem("address_expression").disabled = false;
			gui.elem("address_order").value = "";
			gui.elem("address_user").value = "";
		}

		gui.elem("addressadd").style.display = "block";
	},
	hide_user_form: function() {
		gui.elem("useradd").style.display = "none";
	},
	hide_address_form: function() {
		gui.elem("addressadd").style.display = "none";
	},
	delete_user: function(name) {
		
		if (confirm("Do you really delete this user?") == true) {
			var xhr = ajax.asyncPost(this.api_url + "delete_user", JSON.stringify({ username: name }), function(xhr){
				console.log(JSON.parse(xhr.response));
			});
		}
		this.reload();
	},
	delete_address: function(expression) {
		if (confirm("Do you really delete the address " + expression + "?") == true) {
			var xhr = ajax.asyncPost(this.api_url + "delete_address", JSON.stringify({ expression: expression }), function(xhr) {
				console.log(JSON.parse(xhr.response));
			});
		}
		this.reload();
	},
	save_user: function() {
		var authdata = null;
		var self = this;

		if (gui.elem("user_password").value !== gui.elem("user_password2").value) {
			this.set_status("Password is not the same");
			return;
		}

		var authdata = gui.elem("user_password").value;

		if (authdata == "") {
			authdata = null;
		}

		var user = {
			user_name: gui.elem("user_name").value,
			user_authdata: authdata,
			user_inrouter: gui.elem("user_inrouter").value,
			user_outrouter: gui.elem("user_outrouter").value,
			user_inroute: gui.elem("user_inroute").value,
			user_outroute: gui.elem("user_outroute").value
		};

		if (gui.elem("form_type").value === "new") {
			ajax.asyncPost(this.api_url + "add_user", JSON.stringify({ user: user}), function(xhr) {
				self.set_status(JSON.parse(xhr.response).status);
			});
		} else {
			ajax.asyncPost(this.api_url + "update_user", JSON.stringify({ user: user}), function(xhr) {
				self.set_status(JSON.parse(xhr.response).status);
			});
			if (authdata != null) {
				ajax.asyncPost(this.api_url + "set_password", JSON.stringify({ user: user}), function(xhr) {
					self.set_status(JSON.parse(xhr.response).status);
				});
			}

		}
		this.reload();
		this.hide_user_form();
	},
	set_status: function(message) {
		gui.elem("status").textContent = message;
	},
	save_address: function() {
		var address = {
			address_expression: gui.elem("address_expression").value,
			address_order: gui.elem("address_order").value,
			address_user: gui.elem("address_user").value
		};

		if (gui.elem("form_address_type").value === "new") {
			ajax.asyncPost(this.api_url + "add_address", JSON.stringify({address: address}), function(xhr) {
				console.log(JSON.parse(xhr.response));
			});
		} else {
			ajax.asyncPost(this.api_url + "update_address", JSON.stringify({address: address}), function(xhr) {
				console.log(JSON.parse(xhr.response));
			});
		}
		this.reload();
		this.hide_address_form();
	},
	reload: function() {
		this.get_users();
		this.get_addresses();
	}
};
