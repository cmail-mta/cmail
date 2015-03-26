var cmail = {
	api_url: "server/",
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
	user: {
		/**
		 * Returns a single user object from api
		 * @param name name of the user
		 * @return user object
		 */
		get: function(name) {
			var xhr = ajax.syncPost(cmail.api_url + "users/?get", JSON.stringify({ username: name}));
			var users = JSON.parse(xhr.response).users;

			return users[0];
		},
		/**
		 * Gets all users from api and fills the user table.
		 */
		get_all: function() {
			var self = this;
			ajax.asyncGet(cmail.api_url + "users/?get", function(xhr) {
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

					cmail.modules.forEach(function(module) {
						var col = gui.create("td");
						var cb = gui.createCheckbox(module + "_cb");
						if (user.modules[module]) {
							cb.checked = true;
						}
						cb.disabled = true;
						col.appendChild(cb);
						tr.appendChild(col);
					});

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

			} else {

				gui.elem("form_type").value = "new";
				gui.elem("user_name").value = "";
				gui.elem("user_name").disabled = false;

			}

			gui.elem("user_password").value = "";
			gui.elem("user_password2").value = "";

			gui.elem("useradd").style.display = "block";
		},
		hide_form: function() {
			gui.elem("useradd").style.display = "none";
		},
		delete: function(name) {

			var self = this;
			if (confirm("Do you really delete this user?") == true) {
				var xhr = ajax.asyncPost(cmail.api_url + "users/?delete", JSON.stringify({ username: name }), function(xhr){
					cmail.set_status(JSON.parse(xhr.response).status);
					self.get_all();
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
			};

			if (gui.elem("form_type").value === "new") {
				ajax.asyncPost(cmail.api_url + "users/?add", JSON.stringify({ user: user}), function(xhr) {
					cmail.set_status(JSON.parse(xhr.response).status);
				});
			} else {
				//ajax.asyncPost(cmail.api_url + "users/?update", JSON.stringify({ user: user}), function(xhr) {
				//	cmail.set_status(JSON.parse(xhr.response).status);
				//});

				if (gui.elem("user_password_revoke").checked) {
					authdata = true;
					user["user_authdata"] = null;

				}
				if (authdata) {
					ajax.asyncPost(cmail.api_url + "users?set_password", JSON.stringify({ user: user}), function(xhr) {
						cmail.set_status(JSON.parse(xhr.response).status);
					});
				}

			}
			this.get_all();
			this.hide_form();
		},


	},
	address: {
		get: function(expression) {
			var xhr = ajax.syncPost(cmail.api_url + "addresses/?get", JSON.stringify({ address: expression}));
			var addresses = JSON.parse(xhr.response).addresses;

			return addresses[0];
		},
		get_all: function() {
			var self = this;

			ajax.asyncGet(cmail.api_url + "addresses/?get", function(xhr) {

				var obj  = JSON.parse(xhr.response);

				if (obj.status != "ok") {
					cmail.set_status(obj.status);
					return;
				}

				var addresses = obj.addresses;
				var addresslist = gui.elem("addresslist");
				addresslist.innerHTML = "";
				var last_address = null;
				var next_address = null;
				var address = null;
				var button = null;

				for (var i = 0; i < addresses.length; i++) {

					address = addresses[i];
					last_address = addresses[i - 1];
					next_address = addresses[i + 1];

					var tr = gui.create("tr");
					tr.appendChild(gui.createColumn(address.address_expression));
					tr.appendChild(gui.createColumn(address.address_order));
					tr.appendChild(gui.createColumn(address.address_user));

					var options = gui.create("td");
										if (last_address) {
						options.appendChild(gui.createButton("/\\", self.switch_order, [last_address, address], self));
					} else {
						button = gui.createButton("/\\", function() {}, [], self);
						button.style.visibility = "hidden";
						options.appendChild(button);
					}
					if (next_address) {
						options.appendChild(gui.createButton("\\/", self.switch_order, [next_address, address], self));
					} else {
						button = gui.createButton("\\/", function() {}, [], self);
						button.style.visibility = "hidden";
						options.appendChild(button);
					}
					options.appendChild(gui.createButton("edit", self.show_form, [address.address_expression], self));
					options.appendChild(gui.createButton("delete", self.delete, [address.address_expression], self));

					tr.appendChild(options);
					addresslist.appendChild(tr);
				};
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
			var self = this;
			if (confirm("Do you really delete the address " + expression + "?") == true) {
				ajax.asyncPost(cmail.api_url + "msa/?delete", JSON.stringify({ expression: expression }), function(xhr) {
					cmail.set_status(JSON.parse(xhr.response).status);
					self.get_all();
				});
			}
		},
		save: function() {
			var self = this;
			var address = {
				address_expression: gui.elem("address_expression").value,
				address_order: gui.elem("address_order").value,
				address_user: gui.elem("address_user").value
			};

			if (gui.elem("form_address_type").value === "new") {
				ajax.asyncPost(cmail.api_url + "addresses/?add", JSON.stringify({address: address}), function(xhr) {
					cmail.set_status(JSON.parse(xhr.response).status);
					self.get_all();
				});
			} else {
				ajax.asyncPost(cmail.api_url + "addresses/?update", JSON.stringify({address: address}), function(xhr) {
					cmail.set_status(JSON.parse(xhr.response).status);
					self.get_all();
				});
			}
			this.hide_address_form();
		},
		switch_order: function(address1, address2) {

			var obj = {
				address1: address1,
				address2: address2
			};
			var self = this;

			ajax.asyncPost(cmail.api_url + "addresses/?switch", JSON.stringify(obj), function(xhr) {
				cmail.set_status(JSON.parse(xhr.response).status);
				self.get_all();
			});
		},


	},
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
	msa: {
		get_all: function() {
			var self = this;

			ajax.asyncGet(cmail.api_url + "msa/?get", function(xhr) {
				var obj = JSON.parse(xhr.response);

				if (obj.status != "ok") {
					cmail.set_status(obj.status);
				}

				var msas = obj.msa;

				var body = gui.elem("msalist");
				body.innerHTML = "";
				msas.forEach(function(msa) {

					var tr = gui.create("tr");

					tr.appendChild(gui.createColumn(msa["msa_user"]));
					tr.appendChild(gui.createColumn(msa["msa_inrouter"]));
					tr.appendChild(gui.createColumn(msa["msa_inroute"]));
					tr.appendChild(gui.createColumn(msa["msa_outrouter"]));
					tr.appendChild(gui.createColumn(msa["msa_outroute"]));

					var options = gui.create("td");

					options.appendChild(gui.createButton("edit", self.show_form, [msa["msa_user"]], self));
					options.appendChild(gui.createButton("delete", self.delete, [msa["msa_user"]], self));

					tr.appendChild(options);

					body.appendChild(tr);
				});
			});
		},
		get: function(username) {
			var xhr = ajax.syncPost(cmail.api_url + "msa/?get", JSON.stringify({ msa_user: username }));
			
			var obj = JSON.parse(xhr.response);
			if (obj.status != "ok") {
				cmail.set_status(obj.status);
				return false;
			}

			if (obj.msa.length > 0) {
				return obj.msa[0];
			}

			cmail.set_status("No user found.");
			return false;
		},
		delete: function(username) {
			var self = this;
			if (confirm("Do you really delete the address " + username + "?") == true) {
				ajax.asyncPost(cmail.api_url + "msa/?delete", JSON.stringify({ msa_user: username }), function(xhr) {
					cmail.set_status(JSON.parse(xhr.response).status);
					self.get();
				});
			}

		},
		show_form: function(username) {
			if (username) {
				gui.elem("form_msa_type").value = "edit";
				var msa = this.get(username);

				if (msa === false) {
					return;
				}

				gui.elem("msa_user").value = msa.msa_user;
				gui.elem("msa_user").disabled = true;
				gui.elem("msa_inrouter").value = msa.msa_inrouter;
				gui.elem("msa_inroute").value = msa.msa_inroute;
				gui.elem("msa_outrouter").value = msa.msa_outrouter;
				gui.elem("msa_outroute").value = msa.msa_outroute;

			} else {
				gui.elem("form_msa_type").value = "new";

				gui.elem("msa_user").value = "";
				gui.elem("msa_inrouter").value = "store";
				gui.elem("msa_inroute").value = "";
				gui.elem("msa_outrouter").value = "drop";
				gui.elem("msa_outroute").value = "";
			}

			gui.elem("msaadd").style.display = "block";
		},
		hide_form: function() {
			gui.elem("msaadd").style.display = "none";
		},
		save: function() {
			var msa = {
				msa_user: gui.elem("msa_user").value,
				msa_inrouter: gui.elem("msa_inrouter").value,
				msa_inroute: gui.elem("msa_inroute").value,
				msa_outrouter: gui.elem("msa_outrouter").value,
				msa_outroute: gui.elem("msa_outroute").value
			};
			var url = "";
			if (gui.elem("form_msa_type").value == "new") {
				url = "msa/?add";
			} else {
				url = "msa/?update";
			}
			ajax.asyncPost(cmail.api_url + url, JSON.stringify(msa), function(xhr) {
				cmail.set_status(JSON.parse(xhr.response).status);
			});
			this.hide_form();
			this.get_all();
		},
		test: function() {
			var address = gui.elem("msa_test_input").value;

			if (address === "") {
				cmail.set_status("Mail address is empty.");
				return;
			}

			var obj = {
				address_expression: address,
				address_routing: "inrouter"
			}

			ajax.asyncPost(cmail.api_url + "addresses/?test", JSON.stringify(obj), function(xhr) {
				var resp = JSON.parse(xhr.response);

				if (resp.status !== "ok") {
					cmail.set_status(resp.status);
					return;
				}

				var body = gui.elem("msa_test_steps");
				body.innerHTML = "";
				resp.steps.forEach(function(step, i) {
					var tr = gui.create("tr");

					tr.appendChild(gui.createColumn(i));
					tr.appendChild(gui.createColumn(step));

					body.appendChild(tr);
				});
			});
		}
	},
	tabs: [
		"user",
		"address",
		"msa",
		"test"
		],
	init: function() {
		this.module.get();
		
		// fill router checkboxes
		this.fill_router();

		// we need the user list for auto completion
		this.user.get_all();

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
				self[tab].get_all();
			} else {
				gui.elem(tab).style.display = "none";
			}
		});

		// check for no tab is displayed
		if (test) {
			gui.elem(this.tabs[0]).style.display = "block";
			self.user.get_all();
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
