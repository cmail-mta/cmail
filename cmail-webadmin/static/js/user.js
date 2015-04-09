var cmail = cmail || {};

cmail.user = {
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
			if (obj.status != "ok" && obj.status != "warning") {
				cmail.set_status(obj.status);
				return;
			}

			if (obj.status == "warning") {
				cmail.set_status("WARNING: " + obj.warning);
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
	delete_right: function() {

		var obj = {
			user_name: gui.elem("user_name").value,
			user_right: gui.elem("user_right").value
		}

		var self = this;
		ajax.asyncPost(cmail.api_url + "users/?delete_right", JSON.stringify(obj), function(xhr) {
			cmail.set_status(JSON.parse(xhr.response).status);
			self.show_form(obj.user_name);
		});
	},
	add_right: function() {

		var obj = {
			user_name: gui.elem("user_name").value,
			user_right: gui.elem("user_right").value
		}

		var self = this;
		ajax.asyncPost(cmail.api_url + "users/?add_right", JSON.stringify(obj), function(xhr) {
			cmail.set_status(JSON.parse(xhr.response).status);
			self.show_form(obj.user_name);
		});
	},
	show_form: function(name) {
		var self = this;
		if (name) {
			gui.elem("form_type").value = "edit";
			var user = this.get(name);

			if (!user) {
				cmail.set_status("User not found!");
				return;
			}

			gui.elem("user_name").value = user.user_name;
			gui.elem("user_name").disabled = true;

			user_rights = gui.elem("user_rights");
			user_rights.innerHTML = "";
			user.user_rights.forEach(function(right) {
				var tr = gui.create('tr');
				tr.appendChild(gui.createColumn(right));
				var option = gui.create("td");
				option.appendChild(gui.createButton("delete", self.delete_right, [user.user_name, right], self));

				tr.appendChild(option);
				user_rights.appendChild(tr);
			});

		} else {

			gui.elem("form_type").value = "new";
			gui.elem("user_name").value = "";
			gui.elem("user_name").disabled = false;
			gui.elem("user_rights").innerHTML = "";

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
			var xhr = ajax.asyncPost(cmail.api_url + "users/?delete", JSON.stringify({ user_name: name }), function(xhr){
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
			ajax.asyncPost(cmail.api_url + "users/?add", JSON.stringify(user), function(xhr) {
				cmail.set_status(JSON.parse(xhr.response).status);
			});
		} else {
			if (gui.elem("user_password_revoke").checked) {
				authdata = true;
				user["user_authdata"] = null;

			}
			if (authdata) {
				ajax.asyncPost(cmail.api_url + "users/?set_password", JSON.stringify(user), function(xhr) {
					cmail.set_status(JSON.parse(xhr.response).status);
				});
			}

		}
		this.get_all();
		this.hide_form();
	}
};
