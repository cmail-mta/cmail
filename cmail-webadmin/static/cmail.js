var cmail = {
	api_url: "server/api.php?",
	/**
	 * all routers 
	 */
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
	user: {
		/**
		 * Returns a single user object from api
		 * @param name name of the user
		 * @return user object
		 */
		get: function(name) {
			var xhr = ajax.syncPost(cmail.api_url + "get_user", JSON.stringify({ username: name}));
			var users = JSON.parse(xhr.response).users;

			return users[0];
		},
		/**
		 * Gets all users from api and fills the user table.
		 */
		get_all: function() {
			var self = this;
			ajax.asyncGet(cmail.api_url + "get_users", function(xhr) {
				var obj = JSON.parse(xhr.response)

				// check status
				if (obj.status != "ok") {
					cmail.set_status(obj.status);
					return;
				}

				// set users
				var users = obj.users;
				self.users = users;

				// table element
				var userlist = gui.elem("userlist");
				userlist.innerHTML = "";

				// userlist
				var list = {};
				users.forEach(function(user) {
					var tr = gui.create("tr");
					tr.appendChild(gui.createColumn(user.user_name));
				
					//can login?
					var checkbox_col = gui.create("td");
					var checkbox = gui.createCheckbox("user_can_login");
					if (user.user_login === 1) {
						checkbox.checked = true;
					}
					checkbox.disabled = true;
					checkbox_col.appendChild(checkbox);
					tr.appendChild(checkbox_col);

					// router rows
					tr.appendChild(gui.createColumn(user.user_inrouter));
					tr.appendChild(gui.createColumn(user.user_inroute));
					tr.appendChild(gui.createColumn(user.user_outrouter));
					tr.appendChild(gui.createColumn(user.user_outroute));


					// option buttons
					var options = gui.create("td");
					options.appendChild(gui.createButton("edit", self.show_form, [user.user_name], self));
					options.appendChild(gui.createButton("delete", self.delete, [user.user_name], self));
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
		show_form: function(name) {

			if (name) {
				gui.elem("form_type").value = "edit";
				var user = this.get(name);

				if (!user) {
					cmail.set_status("User not found!");
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
		hide_form: function() {
			gui.elem("useradd").style.display = "none";
		},
		delete: function(name) {

			if (confirm("Do you really delete this user?") == true) {
				var xhr = ajax.asyncPost(cmail.api_url + "delete_user", JSON.stringify({ username: name }), function(xhr){
					cmail.set_status(JSON.parse(xhr.response).status);
					cmail.reload();
				});
			}
		},
		save: function() {
			var authdata = null;
			var self = this;

			if (gui.elem("user_password").value !== gui.elem("user_password2").value) {
				cmail.set_status("Password is not the same");
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
				ajax.asyncPost(cmail.api_url + "add_user", JSON.stringify({ user: user}), function(xhr) {
					cmail.set_status(JSON.parse(xhr.response).status);
				});
			} else {
				ajax.asyncPost(cmail.api_url + "update_user", JSON.stringify({ user: user}), function(xhr) {
					cmail.set_status(JSON.parse(xhr.response).status);
				});


				if (gui.elem("user_password_revoke").checked) {
					authdata = true;
					user["user_authdata"] = null;

				}
				if (authdata) {
					ajax.asyncPost(cmail.api_url + "set_password", JSON.stringify({ user: user}), function(xhr) {
						cmail.set_status(JSON.parse(xhr.response).status);
					});
				}

			}
			cmail.reload();
			this.hide_form();
		},


	},
	address: {
		get: function(expression) {
			var xhr = ajax.syncPost(cmail.api_url + "get_address", JSON.stringify({ address: expression}));
			var addresses = JSON.parse(xhr.response).addresses;

			return addresses[0];
		},
		get_all: function() {
			var self = this;

			ajax.asyncGet(cmail.api_url + "get_addresses", function(xhr) {

				var obj  = JSON.parse(xhr.response);

				if (obj.status != "ok") {
					cmail.set_status(obj.status);
					return;
				}

				var addresses = obj.addresses;
				var addresslist = gui.elem("addresslist");
				addresslist.innerHTML = "";
				var last_address = null;

				addresses.forEach(function(address) {

					var tr = gui.create("tr");
					tr.appendChild(gui.createColumn(address.address_expression));

					var order = gui.create('td');
					order.appendChild(gui.createText(address.address_order));
					if (last_address) {
						order.appendChild(gui.createButton("^", self.switch_order, [last_address, address], self));
					}
					tr.appendChild(order);
					last_address = address;
					tr.appendChild(gui.createColumn(address.address_order));
					tr.appendChild(gui.createColumn(address.address_user));

					var options = gui.create("td");
					options.appendChild(gui.createButton("edit", self.show_form, [address.address_expression], self));
					options.appendChild(gui.createButton("delete", self.delete, [address.address_expression], self));
					tr.appendChild(options);
					addresslist.appendChild(tr);
				});
			});
		},
		show_form: function(expression) {
			if (expression) {
				gui.elem("form_type").value = "edit";

				var address = this.get(expression);

				if (!address) {
					cmail.set_status("Address not found!");
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
		hide_form: function() {
			gui.elem("addressadd").style.display = "none";
		},
		delete: function(expression) {
			if (confirm("Do you really delete the address " + expression + "?") == true) {
				ajax.asyncPost(cmail.api_url + "delete_address", JSON.stringify({ expression: expression }), function(xhr) {
					cmail.set_status(JSON.parse(xhr.response).status);
					cmail.reload();
				});
			}
		},
		save: function() {
			var address = {
				address_expression: gui.elem("address_expression").value,
				address_order: gui.elem("address_order").value,
				address_user: gui.elem("address_user").value
			};

			if (gui.elem("form_address_type").value === "new") {
				ajax.asyncPost(cmail.api_url + "add_address", JSON.stringify({address: address}), function(xhr) {
					cmail.set_status(JSON.parse(xhr.response).status);
					cmail.reload();
				});
			} else {
				ajax.asyncPost(cmail.api_url + "update_address", JSON.stringify({address: address}), function(xhr) {
					cmail.set_status(JSON.parse(xhr.response).status);
					cmail.reload();
				});
			}
			this.hide_address_form();
		},
		switch_order: function(address1, address2) {

			var obj = {
				address1: address1,
				address2: address2
			};
			console.log(obj);
			var self = this;

			ajax.asyncPost(cmail.api_url + "switch_addresses", JSON.stringify(obj), function(xhr) {
				cmail.set_status(JSON.parse(xhr.response).status);
				cmail.reload();
			});
		},


	},
	tabs: [
		"users",
		"addresses"
		],
	init: function() {
		this.reload();
		this.fill_router();
		var self = this;
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
	fill_router: function() {

		var inrouter = gui.elem("user_inrouter");
		var outrouter = gui.elem("user_outrouter");

		this.router.forEach(function(val) {
			inrouter.appendChild(gui.createOption(val, val));
			outrouter.appendChild(gui.createOption(val, val));
		});
	},
	set_status: function(message) {
		gui.elem("status").textContent = message;
	},
	reload: function() {
		this.user.get_all();
		this.address.get_all();
	}
};
